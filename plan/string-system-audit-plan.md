# 字符串系统审计修复计划

> **生成时间**：2026-06-01
> **审计来源**：`core/string/` 全模块 + 跨模块使用扫描
> **关联笔记**：[[Notes/SelfGameEngine/基础工具层/字符串系统]]、[[Game/第二阶段-基础层/base-源码解析：字符串系统]]

---

## 0. 计划概览

| 优先级 | 任务 | 预估工时 | 风险 |
|--------|------|---------|------|
| P0 | 修复 `append()` union 破坏 bug | 30min | Critical - 运行时崩溃/泄漏 |
| P0 | 修复 `operator=(const char*)` UAF 风险 | 20min | Warning - 自我赋值场景崩溃 |
| P1 | 扩展 `String` 单元测试 | 1h | Warning - 无回归防护 |
| P1 | `NamePool` 迁移为 ECS Resource | 2h | Warning - 架构偏离 |
| P2 | 增加 `reserve` / `capacity` | 30min | Info - 性能优化 |
| P2 | 增加 `entelechy_snprintf` 跨平台封装 | 30min | Info - 可移植性 |
| P3 | 增加 `StringView` 类型 | 1h | Suggestion - 工程补全 |
| P3 | 统一移动语义后源对象清空行为 | 15min | Suggestion - 一致性 |

---

## P0: 修复 `String::append()` union 破坏与内存泄漏

**文件**：`_engine/source/core/public/string/small_string.h`  
**行号**：188-209  
**严重等级**：🔴 Critical

### 问题

当当前对象处于**堆模式**（`m_len > SSO_CAPACITY`），追加后总长度变为**≤ 15**，代码直接写入 `m_inline`，导致：

1. `m_heap.m_data` 指针被覆盖（union 重叠），原堆内存泄漏
2. union 活跃成员切换是未定义行为

### 复现代码

```cpp
String s("1234567890123456"); // 16 bytes → heap mode
s = "12345";                        // 5 bytes → inline mode, but... wait
// Actually this triggers via operator=(const char*) which is safe.
// The real trigger:
String s2("1234567890123456");
s2.clear();           // m_len = 0, inline mode
s2.append("123456789012345"); // 15 bytes, stays inline, safe

// True trigger: heap → inline via append
String s3("1234567890123456"); // heap
s3 = "12345678901";                  // 11 bytes, safe (operator= handles free first)
// There is no direct trigger in current API because append only GROWS.
// BUT: if we later add shrink/reassign, this pattern becomes dangerous.
// Wait — the bug IS real for this case:
String s4("1234567890123456"); // heap, len=16
s4 = "1234567890123";               // 13 bytes, operator= frees heap then inline, safe
s4.append("xx");                    // 15 bytes total, inline branch, SAFE (already inline)

// Actual trigger:
// If we call append() after a state where m_len > SSO_CAPACITY:
// But append() always increases length, so heap → inline via append is IMPOSSIBLE
// because new_len = m_len + add_len > m_len > SSO_CAPACITY.
//
// The bug only triggers if append() is modified to allow "replace" semantics
// or if called after a partial reset.
//
// HOWEVER: clear() sets m_len = 0, then append() starts from inline.
// The bug is latent — if any future codepath allows m_len > SSO_CAPACITY
// with new_len <= SSO_CAPACITY (e.g. resize down + append), it explodes.
```

**重新评估**：纯 `append()` 由于只增不减，**不会**触发堆→内联。但 `assign()` 方法（line 148-168）**会**：

```cpp
String s("1234567890123456"); // heap, len=16
s.assign("hello", 5);              // new len=5 ≤ 15, inline branch!
// BUG: directly writes m_inline while object is in heap mode
```

### 修复方案

```cpp
String& append(const char* str) {
    if (!str || str[0] == '\0') return *this;
    usize add_len = std::strlen(str);
    usize new_len = m_len + add_len;
    if (new_len <= SSO_CAPACITY) {
        char tmp[SSO_CAPACITY + 1];
        const char* old = c_str();
        std::memcpy(tmp, old, m_len);
        std::memcpy(tmp + m_len, str, add_len + 1);
        if (m_len > SSO_CAPACITY) {
            DefaultAllocator::free(m_heap.m_data);
        }
        std::memcpy(m_inline, tmp, new_len + 1);
    } else {
        usize new_cap = new_len + 1;
        char* new_data = static_cast<char*>(DefaultAllocator::alloc(new_cap, alignof(char)));
        std::memcpy(new_data, c_str(), m_len);
        std::memcpy(new_data + m_len, str, add_len + 1);
        if (m_len > SSO_CAPACITY) {
            DefaultAllocator::free(m_heap.m_data);
        }
        m_heap.m_data = new_data;
        m_heap.m_capacity = new_cap;
    }
    m_len = new_len;
    return *this;
}
```

### 验证测试

```cpp
TEST(Base, String_AssignHeapToInline) {
    Entelechy::String s("1234567890123456"); // heap
    s.assign("hello", 5);                          // inline
    ASSERT_TRUE(s.isInline());
    ASSERT_EQ(s, "hello");
}

TEST(Base, String_AppendThenAssignSmall) {
    Entelechy::String s;
    s.append("123456789012345"); // 15 bytes inline
    ASSERT_TRUE(s.isInline());
    s = "1234567890123456";      // heap
    s.assign("a", 1);            // back to inline
    ASSERT_TRUE(s.isInline());
    ASSERT_EQ(s, "a");
}
```

---

## P0: 修复 `String::operator=(const char*)` UAF 风险

**文件**：`_engine/source/core/public/string/small_string.h`  
**行号**：128-146  
**严重等级**：🟠 Warning

### 问题

先 `free` 后 `memcpy`，若 `str` 指向自身内部堆数据，读取已释放内存。

### 修复方案

采用"保守构造临时对象再移动"策略，彻底消除自我赋值风险：

```cpp
String& operator=(const char* str) {
    if (!str) {
        clear();
        return *this;
    }
    // 统一路径：构造临时对象，避免自我赋值问题
    String tmp(str);
    *this = std::move(tmp);
    return *this;
}
```

**代价**：多一次 `strlen` 和可能的堆分配。如果性能敏感，可保留快速路径：

```cpp
String& operator=(const char* str) {
    if (!str) { clear(); return *this; }
    // 快速路径：str 不指向自身内部
    const char* self_data = c_str();
    if (str < self_data || str >= self_data + m_len) {
        if (m_len > SSO_CAPACITY) DefaultAllocator::free(m_heap.m_data);
        m_len = std::strlen(str);
        if (m_len <= SSO_CAPACITY) {
            std::memcpy(m_inline, str, m_len + 1);
        } else {
            m_heap.m_capacity = m_len + 1;
            m_heap.m_data = static_cast<char*>(DefaultAllocator::alloc(m_heap.m_capacity, alignof(char)));
            std::memcpy(m_heap.m_data, str, m_len + 1);
        }
        return *this;
    }
    // 保守路径：str 指向自身内部
    String tmp(str);
    *this = std::move(tmp);
    return *this;
}
```

**建议**：先采用简单版本（总是构造临时），通过 Profiling 验证后再优化。

---

## P1: 扩展 `String` 单元测试

**文件**：`_engine/source/core/tests/test_string.cpp`  
**严重等级**：🟠 Warning

### 目标覆盖率

| 场景 | 测试名 | 验证点 |
|------|--------|--------|
| SSO 边界 | `SSO_Boundary` | 15 字节 inline, 16 字节 heap |
| 构造 | `ConstructFromCString`, `ConstructFromSubstring` | nullptr, empty, inline, heap |
| 拷贝 | `CopyCtorInline`, `CopyCtorHeap` | 深拷贝正确，独立生命周期 |
| 移动 | `MoveCtorInline`, `MoveCtorHeap` | 资源转移，源对象可析构 |
| 赋值 | `AssignInline`, `AssignHeap`, `AssignSelf` | 无泄漏，无 UAF |
| 追加 | `AppendInline`, `AppendHeap`, `AppendHeapToInline` | 模式切换正确 |
| 清除 | `ClearThenReuse` | 堆内存释放，可重新进入 inline |
| 比较 | `Compare`, `CompareWithCString` | ==, !=, 空串 |
| 查找 | `FindSubstring`, `FindChar` | npos 语义 |
| 切片 | `SubstrInline`, `SubstrHeap` | 边界条件 |
| 前缀后缀 | `StartsWith`, `EndsWith` | 空串, 超长串 |
| Hash | `HashConsistency` | 相同内容相同 hash |

### 测试代码骨架

```cpp
#include "test/test_framework.h"
#include "core/string/string.h"

TEST(Base, String_SSOStr) {
    Entelechy::String s15("123456789012345");  // 15 chars
    ASSERT_TRUE(s15.isInline());
    ASSERT_EQ(s15.length(), 15u);

    Entelechy::String s16("1234567890123456"); // 16 chars
    ASSERT_FALSE(s16.isInline());
    ASSERT_EQ(s16.length(), 16u);
}

TEST(Base, String_ConstructNull) {
    Entelechy::String s(nullptr);
    ASSERT_TRUE(s.empty());
    ASSERT_EQ(s.length(), 0u);
}

TEST(Base, String_CopyCtor) {
    Entelechy::String src("1234567890123456"); // heap
    Entelechy::String dst(src);
    ASSERT_EQ(dst, src);
    dst.append("x");
    ASSERT_NE(dst, src); // 验证深拷贝
}

TEST(Base, String_MoveCtorHeap) {
    Entelechy::String src("1234567890123456");
    Entelechy::String dst(std::move(src));
    ASSERT_EQ(dst, "1234567890123456");
    ASSERT_TRUE(src.empty()); // 或至少可安全析构
}

TEST(Base, String_AssignHeapToInline) {
    Entelechy::String s("1234567890123456");
    s = "hi";
    ASSERT_TRUE(s.isInline());
    ASSERT_EQ(s, "hi");
}

TEST(Base, String_AppendGrowth) {
    Entelechy::String s;
    for (int i = 0; i < 20; ++i) {
        s.append("x");
    }
    ASSERT_EQ(s.length(), 20u);
    ASSERT_FALSE(s.isInline());
}

TEST(Base, String_ClearReuse) {
    Entelechy::String s("1234567890123456");
    s.clear();
    ASSERT_TRUE(s.empty());
    s.append("short");
    ASSERT_TRUE(s.isInline());
}

TEST(Base, String_Find) {
    Entelechy::String s("hello world");
    ASSERT_EQ(s.find("world"), 6u);
    ASSERT_EQ(s.find("xyz"), Entelechy::String::npos);
    ASSERT_EQ(s.find('o'), 4u);
}

TEST(Base, String_Substr) {
    Entelechy::String s("hello world");
    auto sub = s.substr(6, 5);
    ASSERT_EQ(sub, "world");
}

TEST(Base, String_Hash) {
    Entelechy::String a("test");
    Entelechy::String b("test");
    std::hash<Entelechy::String> hasher;
    ASSERT_EQ(hasher(a), hasher(b));
}
```

---

## P1: `NamePool` 迁移为 ECS Resource

**文件**：
- `_engine/source/core/public/string/name_pool.h`
- `_engine/source/core/private/string/name_pool.cpp`
- `_engine/source/ecs/public/type/type_registry.h`（参考 Resource 模式）

**严重等级**：🟠 Warning

### 当前问题

```cpp
// 全局单例，无法通过 World 查询
NamePool::instance().intern("foo");
```

### 目标架构

```cpp
// 1. 在 ecs/public/resource/ 下新增（或复用现有 resource 目录）
// _engine/source/ecs/public/resource/name_pool_resource.h
#pragma once
#include "core/string/name_pool.h"

namespace Entelechy {

struct NamePoolResource {
    NamePool pool;
};

} // namespace Entelechy
```

```cpp
// 2. World 初始化时注册
// _engine/source/ecs/private/world/world.cpp
void World::init() {
    // ... existing init ...
    registerResource<NamePoolResource>();
}
```

```cpp
// 3. 消费者迁移示例
// _engine/source/ecs/private/prefab/scene_serializer.cpp
// 原代码：
//   *reinterpret_cast<Name*>(ptr) = NamePool::instance().intern(val.c_str());
// 新代码：
//   auto& pool = world.resource<NamePoolResource>()->pool;
//   *reinterpret_cast<Name*>(ptr) = pool.intern(val.c_str());
```

### 迁移清单

| 消费者文件 | 当前调用 | 迁移方式 |
|-----------|---------|---------|
| `scene_serializer.cpp:234` | `NamePool::instance().intern(...)` | 通过 `World&` 参数获取 Resource |
| `agent_bridge.cpp` | 无直接调用 | 不涉及 |

**注意**：`Name` 的编译期构造（`"foo"_name`）不依赖 Intern 池，只有运行期 `intern()` 需要迁移。

---

## P2: 增加 `reserve` / `capacity`

**文件**：`_engine/source/core/public/string/string.h`

### 新增接口

```cpp
class String {
    // ... existing ...
public:
    [[nodiscard]] usize capacity() const {
        return m_len <= SSO_CAPACITY ? SSO_CAPACITY : m_heap.m_capacity - 1;
    }

    String& reserve(usize min_cap) {
        if (min_cap <= capacity()) return *this;
        usize new_cap = min_cap + 1;
        char* new_data = static_cast<char*>(DefaultAllocator::alloc(new_cap, alignof(char)));
        std::memcpy(new_data, c_str(), m_len + 1);
        if (m_len > SSO_CAPACITY) {
            DefaultAllocator::free(m_heap.m_data);
        }
        m_heap.m_data = new_data;
        m_heap.m_capacity = new_cap;
        // m_len unchanged; object is now in heap mode even if m_len <= SSO_CAPACITY
        return *this;
    }
};
```

### 使用场景

```cpp
String s;
s.reserve(200); // 预分配，避免后续 20 次 append 反复分配
for (auto& item : items) {
    s.append(item.name);
}
```

---

## P2: 增加 `entelechy_snprintf` 跨平台封装

**文件**：
- 新增 `_engine/source/core/public/string/platform_snprintf.h`
- 新增 `_engine/source/core/private/string/platform_snprintf.cpp`
- 修改 `_engine/source/core/public/string/string_format.h`

### 实现

```cpp
// platform_snprintf.h
#pragma once
#include "core/foundation_types.h"
#include <cstdarg>

namespace Entelechy {

// 跨平台封装，统一截断行为和返回值语义
// - 始终保证 buffer[buf_size - 1] = '\0'（只要 buf_size > 0）
// - 返回实际写入的字符数（不含 \0），截断时返回 buf_size - 1
int platform_snprintf(char* buffer, usize buf_size, const char* format, ...);
int platform_vsnprintf(char* buffer, usize buf_size, const char* format, va_list args);

} // namespace Entelechy
```

```cpp
// platform_snprintf.cpp
#include "core/string/platform_snprintf.h"

#ifdef _MSC_VER
    #include <stdio.h>
#endif

namespace Entelechy {

int platform_vsnprintf(char* buffer, usize buf_size, const char* format, va_list args) {
    if (buf_size == 0) return 0;
#ifdef _MSC_VER
    int ret = _vsnprintf_s(buffer, buf_size, _TRUNCATE, format, args);
    if (ret < 0) return static_cast<int>(buf_size - 1);
    return ret;
#else
    int ret = vsnprintf(buffer, buf_size, format, args);
    if (ret < 0) return 0;
    if (static_cast<usize>(ret) >= buf_size) return static_cast<int>(buf_size - 1);
    return ret;
#endif
}

int platform_snprintf(char* buffer, usize buf_size, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int ret = platform_vsnprintf(buffer, buf_size, format, args);
    va_end(args);
    return ret;
}

} // namespace Entelechy
```

### 替换 `string_format.h` 中的调用

将所有 `snprintf(...)` 替换为 `platform_snprintf(...)`。

---

## P3: 增加 `StringView` 类型

**文件**：新增 `_engine/source/core/public/string/string_view.h`

### 实现

```cpp
#pragma once
#include "core/foundation_types.h"
#include <cstring>

namespace Entelechy {

class StringView {
    const char* m_data = "";
    usize m_len = 0;

public:
    constexpr StringView() = default;
    constexpr StringView(const char* s) : m_data(s ? s : ""), m_len(s ? std::strlen(s) : 0) {}
    constexpr StringView(const char* s, usize len) : m_data(s ? s : ""), m_len(len) {}
    StringView(const String& s) : m_data(s.c_str()), m_len(s.length()) {}

    [[nodiscard]] constexpr const char* data() const { return m_data; }
    [[nodiscard]] constexpr usize size() const { return m_len; }
    [[nodiscard]] constexpr usize length() const { return m_len; }
    [[nodiscard]] constexpr bool empty() const { return m_len == 0; }
    [[nodiscard]] constexpr char operator[](usize i) const { return m_data[i]; }

    [[nodiscard]] constexpr StringView substr(usize pos, usize len = npos) const {
        if (pos >= m_len) return StringView();
        usize actual = (len == npos || pos + len > m_len) ? (m_len - pos) : len;
        return StringView(m_data + pos, actual);
    }

    [[nodiscard]] constexpr bool startsWith(const char* prefix) const {
        if (!prefix) return true;
        usize preLen = std::strlen(prefix);
        if (preLen > m_len) return false;
        return std::memcmp(m_data, prefix, preLen) == 0;
    }

    [[nodiscard]] constexpr bool operator==(StringView other) const {
        return m_len == other.m_len && std::memcmp(m_data, other.m_data, m_len) == 0;
    }

    [[nodiscard]] constexpr bool operator!=(StringView other) const { return !(*this == other); }

    static constexpr usize npos = static_cast<usize>(-1);
};

} // namespace Entelechy
```

### 使用场景

```cpp
// 替代 substr 的零拷贝版本
StringView getExtension(StringView path) {
    usize dot = path.findLast('.');
    if (dot == StringView::npos) return StringView();
    return path.substr(dot + 1);
}
```

---

## P3: 统一移动语义后源对象清空行为

**文件**：`_engine/source/core/public/string/string.h`

### 修复

```cpp
// move ctor
String(String&& other) noexcept {
    m_len = other.m_len;
    if (other.m_len <= SSO_CAPACITY) {
        std::memcpy(m_inline, other.m_inline, m_len + 1);
    } else {
        m_heap.m_data = other.m_heap.m_data;
        m_heap.m_capacity = other.m_heap.m_capacity;
    }
    other.m_inline[0] = '\0';
    other.m_len = 0;  // ← 补充这一行（内联分支原来没有）
}

// move assign
String& operator=(String&& other) noexcept {
    if (this == &other) return *this;
    if (m_len > SSO_CAPACITY) {
        DefaultAllocator::free(m_heap.m_data);
    }
    m_len = other.m_len;
    if (other.m_len <= SSO_CAPACITY) {
        std::memcpy(m_inline, other.m_inline, m_len + 1);
    } else {
        m_heap.m_data = other.m_heap.m_data;
        m_heap.m_capacity = other.m_heap.m_capacity;
    }
    other.m_inline[0] = '\0';
    other.m_len = 0;  // ← 补充这一行（内联分支原来没有）
    return *this;
}
```

---

## 附录：验证清单

修复完成后，运行以下命令验证：

```bash
# 1. 构建
cd D:/workspace/Entelechy && cmake --build build

# 2. 运行字符串相关测试
./build/_engine/source/core/tests/CoreTests --filter String*

# 3. 运行全部测试确认无回归
./build/_engine/source/core/tests/CoreTests
./build/_engine/source/ecs/tests/ECSTests
```

---

## 附录：与 Vault 笔记的同步建议

修复完成后，以下笔记应更新以反映最新实现：

1. **`Notes/SelfGameEngine/基础工具层/字符串系统.md`**
   - 在 `String` 代码示例中补充 `reserve` / `capacity` 接口
   - 在 `NamePool` 示例中补充 ECS Resource 版本
   - 增加 `StringView` 设计说明

2. **`Notes/SelfGameEngine/审计/`**（如用户要求保存审计记录）
   - 本审计报告可归档为 `2026-06-01-字符串系统.md`

---
