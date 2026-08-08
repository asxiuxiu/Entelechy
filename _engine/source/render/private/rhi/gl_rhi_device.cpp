#include "render/rhi/gl_rhi_device.h"
#include "render/rhi/render_command_buffer.h"
#include "render/rhi/deferred_command_list.h"
#include "render/rhi/gl_command_translator.h"
#include "window/window.h"
#include "log/core/log_macros.h"
#include "core/allocator/allocator.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cstring>
#include <memory>
#include <vector>

namespace Entelechy
{

namespace
{
template <typename T, typename... Args>
T *allocateResource(Args &&...args)
{
    void *mem = DefaultAllocator::alloc(sizeof(T), alignof(T));
    return new (mem) T(std::forward<Args>(args)...);
}
} // namespace

// ==================================================================
// Helper: TextureFormat -> OpenGL format mapping
// ==================================================================
struct GLFormatInfo
{
    GLint internalFormat;
    GLenum format;
    GLenum type;
};

static u32 getTextureFormatBytesPerPixel(TextureFormat fmt)
{
    switch (fmt)
    {
    case TextureFormat::R8_UNORM:
        return 1;
    case TextureFormat::RG8_UNORM:
        return 2;
    case TextureFormat::RGBA8_UNORM:
    case TextureFormat::RGBA8_SRGB:
    case TextureFormat::BGRA8_UNORM:
        return 4;
    case TextureFormat::R16_FLOAT:
        return 2;
    case TextureFormat::RG16_FLOAT:
        return 4;
    case TextureFormat::RGBA16_FLOAT:
        return 8;
    case TextureFormat::R32_FLOAT:
        return 4;
    case TextureFormat::RG32_FLOAT:
        return 8;
    case TextureFormat::RGBA32_FLOAT:
        return 16;
    case TextureFormat::D24_UNORM_S8_UINT:
        return 4;
    case TextureFormat::D32_FLOAT:
        return 4;
    default:
        return 4;
    }
}

static GLFormatInfo getGLFormatInfo(TextureFormat fmt)
{
    switch (fmt)
    {
    case TextureFormat::R8_UNORM:
        return {GL_R8, GL_RED, GL_UNSIGNED_BYTE};
    case TextureFormat::RG8_UNORM:
        return {GL_RG8, GL_RG, GL_UNSIGNED_BYTE};
    case TextureFormat::RGBA8_UNORM:
        return {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE};
    case TextureFormat::RGBA8_SRGB:
        return {GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_BYTE};
    case TextureFormat::R16_FLOAT:
        return {GL_R16F, GL_RED, GL_HALF_FLOAT};
    case TextureFormat::RG16_FLOAT:
        return {GL_RG16F, GL_RG, GL_HALF_FLOAT};
    case TextureFormat::RGBA16_FLOAT:
        return {GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT};
    case TextureFormat::R32_FLOAT:
        return {GL_R32F, GL_RED, GL_FLOAT};
    case TextureFormat::RG32_FLOAT:
        return {GL_RG32F, GL_RG, GL_FLOAT};
    case TextureFormat::RGBA32_FLOAT:
        return {GL_RGBA32F, GL_RGBA, GL_FLOAT};
    case TextureFormat::D24_UNORM_S8_UINT:
        return {GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8};
    case TextureFormat::D32_FLOAT:
        return {GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT};
    default:
        return {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE};
    }
}

static GLenum getGLTopology(PrimitiveTopology topo)
{
    switch (topo)
    {
    case PrimitiveTopology::Triangles:
        return GL_TRIANGLES;
    case PrimitiveTopology::TriangleStrip:
        return GL_TRIANGLE_STRIP;
    case PrimitiveTopology::Lines:
        return GL_LINES;
    case PrimitiveTopology::LineStrip:
        return GL_LINE_STRIP;
    case PrimitiveTopology::Points:
        return GL_POINTS;
    default:
        return GL_TRIANGLES;
    }
}

static GLenum getGLCullMode(CullMode mode)
{
    switch (mode)
    {
    case CullMode::None:
        return GL_NONE;
    case CullMode::Front:
        return GL_FRONT;
    case CullMode::Back:
        return GL_BACK;
    default:
        return GL_BACK;
    }
}

static GLenum getGLFrontFace(FrontFace face)
{
    switch (face)
    {
    case FrontFace::CounterClockwise:
        return GL_CCW;
    case FrontFace::Clockwise:
        return GL_CW;
    default:
        return GL_CCW;
    }
}

static GLenum getGLCompareFunc(CompareFunc fn)
{
    switch (fn)
    {
    case CompareFunc::Never:
        return GL_NEVER;
    case CompareFunc::Less:
        return GL_LESS;
    case CompareFunc::Equal:
        return GL_EQUAL;
    case CompareFunc::LessEqual:
        return GL_LEQUAL;
    case CompareFunc::Greater:
        return GL_GREATER;
    case CompareFunc::NotEqual:
        return GL_NOTEQUAL;
    case CompareFunc::GreaterEqual:
        return GL_GEQUAL;
    case CompareFunc::Always:
        return GL_ALWAYS;
    default:
        return GL_LESS;
    }
}

static GLenum getGLBlendFactor(BlendFactor f)
{
    switch (f)
    {
    case BlendFactor::Zero:
        return GL_ZERO;
    case BlendFactor::One:
        return GL_ONE;
    case BlendFactor::SrcColor:
        return GL_SRC_COLOR;
    case BlendFactor::OneMinusSrcColor:
        return GL_ONE_MINUS_SRC_COLOR;
    case BlendFactor::SrcAlpha:
        return GL_SRC_ALPHA;
    case BlendFactor::OneMinusSrcAlpha:
        return GL_ONE_MINUS_SRC_ALPHA;
    case BlendFactor::DstColor:
        return GL_DST_COLOR;
    case BlendFactor::OneMinusDstColor:
        return GL_ONE_MINUS_DST_COLOR;
    case BlendFactor::DstAlpha:
        return GL_DST_ALPHA;
    case BlendFactor::OneMinusDstAlpha:
        return GL_ONE_MINUS_DST_ALPHA;
    default:
        return GL_ONE;
    }
}

static GLenum getGLBlendOp(BlendOp op)
{
    switch (op)
    {
    case BlendOp::Add:
        return GL_FUNC_ADD;
    case BlendOp::Subtract:
        return GL_FUNC_SUBTRACT;
    case BlendOp::ReverseSubtract:
        return GL_FUNC_REVERSE_SUBTRACT;
    case BlendOp::Min:
        return GL_MIN;
    case BlendOp::Max:
        return GL_MAX;
    default:
        return GL_FUNC_ADD;
    }
}

static GLenum getGLShaderStage(ShaderStage stage)
{
    switch (stage)
    {
    case ShaderStage::Vertex:
        return GL_VERTEX_SHADER;
    case ShaderStage::Fragment:
        return GL_FRAGMENT_SHADER;
    case ShaderStage::Geometry:
        return GL_GEOMETRY_SHADER;
    default:
        return GL_VERTEX_SHADER;
    }
}

// ==================================================================
// GLBuffer
// ==================================================================
GLBuffer::GLBuffer(u32 size, BufferUsage usage, GLuint vbo, GLuint vao, void *mapped)
    : m_size(size), m_usage(usage), m_vbo(vbo), m_vao(vao), m_mapped(mapped)
{
}

GLBuffer::~GLBuffer()
{
    onDestroy();
}

void GLBuffer::onDestroy()
{
    if (m_mapped)
    {
        // Unmap before deleting the buffer object (context must be current).
        glBindBuffer(GL_UNIFORM_BUFFER, m_vbo);
        glUnmapBuffer(GL_UNIFORM_BUFFER);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
        m_mapped = nullptr;
    }
    if (m_vao)
    {
        glDeleteVertexArrays(1, &m_vao);
        m_vao = 0;
    }
    if (m_vbo)
    {
        glDeleteBuffers(1, &m_vbo);
        m_vbo = 0;
    }
}

void GLBuffer::setDebugName(const String &name)
{
#if defined(GLAD_GL_KHR_debug)
    if (m_vbo && !name.empty())
    {
        glObjectLabelKHR(GL_BUFFER_KHR, m_vbo, static_cast<GLsizei>(name.length()), name.c_str());
    }
#elif defined(GLAD_GL_VERSION_4_3)
    if (m_vbo && !name.empty())
    {
        glObjectLabel(GL_BUFFER, m_vbo, static_cast<GLsizei>(name.length()), name.c_str());
    }
#endif
}

// ==================================================================
// GLTexture
// ==================================================================
GLTexture::GLTexture(const TextureDesc &desc, GLuint texture, GLenum target)
    : m_desc(desc), m_texture(texture), m_target(target)
{
}

GLTexture::~GLTexture()
{
    onDestroy();
}

void GLTexture::onDestroy()
{
    if (m_texture)
    {
        glDeleteTextures(1, &m_texture);
        m_texture = 0;
    }
}

u64 GLTexture::memorySizeBytes() const
{
    return GLRHIDevice::textureMemorySizeBytes(m_desc);
}

void GLTexture::setDebugName(const String &name)
{
#if defined(GLAD_GL_KHR_debug)
    if (m_texture && !name.empty())
    {
        glObjectLabelKHR(GL_TEXTURE_KHR, m_texture, static_cast<GLsizei>(name.length()), name.c_str());
    }
#elif defined(GLAD_GL_VERSION_4_3)
    if (m_texture && !name.empty())
    {
        glObjectLabel(GL_TEXTURE, m_texture, static_cast<GLsizei>(name.length()), name.c_str());
    }
#endif
}

// ==================================================================
// GLShader
// ==================================================================
GLShader::GLShader(ShaderStage stage, GLuint shader) : m_stage(stage), m_shader(shader) {}

GLShader::~GLShader()
{
    onDestroy();
}

void GLShader::onDestroy()
{
    if (m_shader)
    {
        glDeleteShader(m_shader);
        m_shader = 0;
    }
}

void GLShader::setDebugName(const String &name)
{
#if defined(GLAD_GL_KHR_debug)
    if (m_shader && !name.empty())
    {
        glObjectLabelKHR(GL_SHADER_KHR, m_shader, static_cast<GLsizei>(name.length()), name.c_str());
    }
#elif defined(GLAD_GL_VERSION_4_3)
    if (m_shader && !name.empty())
    {
        glObjectLabel(GL_SHADER, m_shader, static_cast<GLsizei>(name.length()), name.c_str());
    }
#endif
}

// ==================================================================
// GLPipelineState
// ==================================================================
GLPipelineState::GLPipelineState(const PipelineStateDesc &desc, GLuint program) : m_desc(desc), m_program(program) {}

GLPipelineState::~GLPipelineState()
{
    onDestroy();
}

void GLPipelineState::onDestroy()
{
    if (m_program)
    {
        glDeleteProgram(m_program);
        m_program = 0;
    }
}

void GLPipelineState::setDebugName(const String &name)
{
#if defined(GLAD_GL_KHR_debug)
    if (m_program && !name.empty())
    {
        glObjectLabelKHR(GL_PROGRAM_KHR, m_program, static_cast<GLsizei>(name.length()), name.c_str());
    }
#elif defined(GLAD_GL_VERSION_4_3)
    if (m_program && !name.empty())
    {
        glObjectLabel(GL_PROGRAM, m_program, static_cast<GLsizei>(name.length()), name.c_str());
    }
#endif
}

// ==================================================================
// GLRHIDevice::DeferredState — holds the deferred command buffer,
// recorder, and translator. Defined here so the header only needs
// forward declarations.
// ==================================================================

struct GLRHIDevice::DeferredState
{
    RenderCommandBuffer buffer;
    DeferredCommandList recorder;
    GLCommandTranslator translator;

    explicit DeferredState(usize capacity)
        : buffer(capacity), recorder(buffer)
    {
    }
};

// ==================================================================
// GLFence
// ==================================================================

GLFence::GLFence(RHIFenceValue initialValue) : m_signaled_value(initialValue) {}

GLFence::~GLFence()
{
    if (m_sync)
    {
        glDeleteSync(m_sync);
        m_sync = nullptr;
    }
}

void GLFence::signal(RHIFenceValue value)
{
    // Delete any previous sync object before creating a new one.
    if (m_sync)
    {
        glDeleteSync(m_sync);
    }
    m_sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    m_signaled_value = value;
}

bool GLFence::wait(RHIFenceValue value, u64 timeoutNs)
{
    if (!m_sync || value > m_signaled_value)
        return false;

    // Convert nanoseconds to microseconds for glClientWaitSync.
    // GL_TIMEOUT_IGNORED maps to UINT64_MAX.
    GLuint64 timeoutUs = (timeoutNs == UINT64_MAX) ? GL_TIMEOUT_IGNORED : static_cast<GLuint64>(timeoutNs / 1000);
    GLenum result = glClientWaitSync(m_sync, GL_SYNC_FLUSH_COMMANDS_BIT, timeoutUs);
    return (result == GL_ALREADY_SIGNALED || result == GL_CONDITION_SATISFIED);
}

RHIFenceValue GLFence::getCompletedValue() const
{
    if (!m_sync)
        return m_signaled_value;

    GLenum result = glClientWaitSync(m_sync, 0, 0);
    if (result == GL_ALREADY_SIGNALED || result == GL_CONDITION_SATISFIED)
    {
        return m_signaled_value;
    }
    // Not yet signaled — return the last known completed value (0 initially).
    return 0;
}

bool GLFence::isSignaled(RHIFenceValue value) const
{
    if (value > m_signaled_value)
        return false;
    if (!m_sync)
        return true;

    GLenum result = glClientWaitSync(m_sync, 0, 0);
    return (result == GL_ALREADY_SIGNALED || result == GL_CONDITION_SATISFIED);
}

void GLFence::onDestroy()
{
    if (m_sync)
    {
        glDeleteSync(m_sync);
        m_sync = nullptr;
    }
}

// ==================================================================
// GLRHIDevice
// ==================================================================

GLRHIDevice::GLRHIDevice(IWindow *window)
    : m_window(window),
      m_deferred(std::make_unique<DeferredState>(4 * 1024 * 1024))
{
}

GLRHIDevice::~GLRHIDevice()
{
    if (m_initialized)
    {
        shutdown();
    }
}

u64 GLRHIDevice::textureMemorySizeBytes(const TextureDesc &desc)
{
    const u32 bpp = getTextureFormatBytesPerPixel(desc.format);
    u64 total = 0;
    u32 w = desc.width;
    u32 h = desc.height;
    u32 d = desc.depth;
    for (u32 mip = 0; mip < desc.mipLevels; ++mip)
    {
        total += static_cast<u64>(w) * h * d * bpp;
        if (w > 1)
            w /= 2;
        if (h > 1)
            h /= 2;
        if (d > 1)
            d /= 2;
    }
    return total * desc.arrayLayers;
}

void GLRHIDevice::trackResourceCreated(const GPUResource *resource)
{
    if (resource)
    {
        m_tracked_memory_bytes += resource->memorySizeBytes();
    }
}

void GLRHIDevice::trackResourceDestroyed(const GPUResource *resource)
{
    if (resource)
    {
        const u64 size = resource->memorySizeBytes();
        m_tracked_memory_bytes = (size > m_tracked_memory_bytes) ? 0 : m_tracked_memory_bytes - size;
    }
}

bool GLRHIDevice::initialize()
{
    if (m_initialized)
        return true;
    m_initialized = true;
    LOG_INFO(LogCategories::kEngine, "GLRHIDevice initialized (OpenGL RHI)");
    return true;
}

void GLRHIDevice::shutdown()
{
    // Release cached pipeline states first so they enter the pending-delete queue.
    m_pso_manager.clear();

    // At shutdown we cannot wait for GPU fences; destroy every queued resource
    // immediately. This is safe because the context is being torn down and no
    // further frames will be submitted.
    for (auto &pd : m_pending_deletes)
    {
        trackResourceDestroyed(pd.resource);
        pd.resource->internalDestroy();
    }
    m_pending_deletes.clear();

    // Delete any frame fences that are still outstanding.
    for (auto &ff : m_frame_fences)
    {
        if (ff.sync)
        {
            glDeleteSync(ff.sync);
            ff.sync = nullptr;
        }
    }
    m_frame_fences.clear();

    m_tracked_memory_bytes = 0;
    m_initialized = false;
}

RHIBufferRef GLRHIDevice::createBuffer(const BufferDesc &desc, const void *initialData)
{
    GLuint bufferObj = 0;
    glGenBuffers(1, &bufferObj);
    if (!bufferObj)
    {
        LOG_ERROR(LogCategories::kEngine, "glGenBuffers failed");
        return nullptr;
    }

    GLenum target = GL_ARRAY_BUFFER;
    if (hasFlag(desc.usage, BufferUsage::Index))
    {
        target = GL_ELEMENT_ARRAY_BUFFER;
    }
    else if (hasFlag(desc.usage, BufferUsage::Uniform))
    {
        target = GL_UNIFORM_BUFFER;
    }

    glBindBuffer(target, bufferObj);

    void *mapped = nullptr;
    if (desc.cpuAccessible)
    {
        // CPU-writable buffer (ConstantBufferRing): persistent map so the
        // CPU can memcpy into it every frame without map/unmap overhead.
        // Prefer glBufferStorage (GL 4.4 / ARB_buffer_storage) with
        // persistent+coherent mapping; fall back to a single long-lived
        // glMapBufferRange on GL_DYNAMIC_DRAW when unavailable.
#if defined(GLAD_GL_VERSION_4_4) || defined(GLAD_GL_ARB_buffer_storage)
        glBufferStorage(target, desc.size, initialData, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
        mapped = glMapBufferRange(target, 0, desc.size, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
#else
        glBufferData(target, desc.size, initialData, GL_DYNAMIC_DRAW);
        mapped = glMapBufferRange(target, 0, desc.size, GL_MAP_WRITE_BIT);
#endif
        if (!mapped)
        {
            LOG_ERROR(LogCategories::kEngine, "GL createBuffer: persistent map failed (%u bytes)", desc.size);
            glDeleteBuffers(1, &bufferObj);
            return nullptr;
        }
    }
    else
    {
        glBufferData(target, desc.size, initialData, GL_STATIC_DRAW);
    }
    glBindBuffer(target, 0);

    GLuint vao = 0;
    // If this is a vertex buffer with layout info, create and configure a VAO
    if (hasFlag(desc.usage, BufferUsage::Vertex) && desc.vertexAttributes && desc.vertexAttributeCount > 0)
    {
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, bufferObj);

        for (u32 i = 0; i < desc.vertexAttributeCount; ++i)
        {
            const auto &attr = desc.vertexAttributes[i];
            glEnableVertexAttribArray(attr.location);
            glVertexAttribPointer(attr.location, static_cast<GLint>(attr.components), GL_FLOAT,
                                  attr.normalized ? GL_TRUE : GL_FALSE, static_cast<GLsizei>(desc.vertexStride),
                                  reinterpret_cast<const void *>(static_cast<uintptr_t>(attr.offset)));
        }

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    auto *buffer = allocateResource<GLBuffer>(desc.size, desc.usage, bufferObj, vao, mapped);
    buffer->setDevice(this);
    trackResourceCreated(buffer);
    return RHIBufferRef(buffer);
}

RHITextureRef GLRHIDevice::createTexture(const TextureDesc &desc, const void *initialData)
{
    GLuint tex = 0;
    glGenTextures(1, &tex);
    if (!tex)
    {
        LOG_ERROR(LogCategories::kEngine, "glGenTextures failed");
        return nullptr;
    }

    auto info = getGLFormatInfo(desc.format);

    if (desc.depth == 1)
    {
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, info.internalFormat, static_cast<GLsizei>(desc.width),
                     static_cast<GLsizei>(desc.height), 0, info.format, info.type, initialData);

        if (desc.mipLevels > 1)
        {
            glGenerateMipmap(GL_TEXTURE_2D);
        }

        // Default sampler params
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, desc.mipLevels > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glBindTexture(GL_TEXTURE_2D, 0);
        auto *texture = allocateResource<GLTexture>(desc, tex, GL_TEXTURE_2D);
        texture->setDevice(this);
        trackResourceCreated(texture);
        return RHITextureRef(texture);
    }
    else
    {
        // 3D texture (simplified path)
        glBindTexture(GL_TEXTURE_3D, tex);
        glTexImage3D(GL_TEXTURE_3D, 0, info.internalFormat, static_cast<GLsizei>(desc.width),
                     static_cast<GLsizei>(desc.height), static_cast<GLsizei>(desc.depth), 0, info.format, info.type,
                     initialData);
        glBindTexture(GL_TEXTURE_3D, 0);
        auto *texture = allocateResource<GLTexture>(desc, tex, GL_TEXTURE_3D);
        texture->setDevice(this);
        trackResourceCreated(texture);
        return RHITextureRef(texture);
    }
}

RHIShaderRef GLRHIDevice::createShader(const ShaderBytecode &bytecode)
{
    if (bytecode.format != ShaderBytecodeFormat::GLSL)
    {
        LOG_ERROR(LogCategories::kEngine,
                  "GLRHIDevice::createShader: unsupported format %u (only GLSL supported)",
                  static_cast<u32>(bytecode.format));
        return nullptr;
    }

    GLenum glStage = getGLShaderStage(bytecode.stage);
    GLuint shader = glCreateShader(glStage);
    if (!shader)
    {
        LOG_ERROR(LogCategories::kEngine, "glCreateShader failed");
        return nullptr;
    }

    const GLchar *source = static_cast<const GLchar *>(bytecode.data);
    GLint length = static_cast<GLint>(bytecode.size);
    glShaderSource(shader, 1, &source, &length);
    glCompileShader(shader);

    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        LOG_ERROR(LogCategories::kEngine, "Shader compile error: %s", log);
        glDeleteShader(shader);
        return nullptr;
    }

    auto *glShader = allocateResource<GLShader>(bytecode.stage, shader);
    glShader->setDevice(this);
    trackResourceCreated(glShader);
    return RHIShaderRef(glShader);
}

RHIPipelineStateRef GLRHIDevice::createPipelineState(const PipelineStateDesc &desc)
{
    // Route PSO creation through the cache. Identical descs (same
    // shader pair + state) share one GL program — before this, every Material
    // linked its own program and the PSOManager below was dead code.
    if (RHIPipelineStateRef cached = m_pso_manager.find(desc))
        return cached;

    RHIPipelineStateRef pso = createPipelineStateUncached(desc);
    if (pso)
        m_pso_manager.insert(desc, pso);
    return pso;
}

RHIFenceRef GLRHIDevice::createFence(RHIFenceValue initialValue)
{
    auto *fence = allocateResource<GLFence>(initialValue);
    fence->setDevice(this);
    trackResourceCreated(fence);
    return RHIFenceRef(fence);
}

RHIPipelineStateRef GLRHIDevice::createPipelineStateUncached(const PipelineStateDesc &desc)
{
    GLuint program = glCreateProgram();
    if (!program)
    {
        LOG_ERROR(LogCategories::kEngine, "glCreateProgram failed");
        return nullptr;
    }

    if (desc.vertexShader)
    {
        auto *glVs = static_cast<GLShader *>(desc.vertexShader);
        glAttachShader(program, glVs->getShader());
    }
    if (desc.fragmentShader)
    {
        auto *glFs = static_cast<GLShader *>(desc.fragmentShader);
        glAttachShader(program, glFs->getShader());
    }

    glLinkProgram(program);

    GLint success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success)
    {
        char log[512];
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        LOG_ERROR(LogCategories::kEngine, "Program link error: %s", log);
        glDeleteProgram(program);
        return nullptr;
    }

    auto *pso = allocateResource<GLPipelineState>(desc, program);
    pso->setDevice(this);
    trackResourceCreated(pso);
    return RHIPipelineStateRef(pso);
}

IRHICommandList *GLRHIDevice::createCommandList()
{
    // Reset the command buffer for a new frame and return the recorder.
    m_deferred->buffer.reset();
    m_deferred->recorder.begin();
    return &m_deferred->recorder;
}

void GLRHIDevice::submit(IRHICommandList *cmdList)
{
    if (!cmdList)
        return;
    cmdList->end();
    // Replay all recorded commands through the GL translator.
    m_deferred->translator.execute(m_deferred->buffer);
}

// -- Surface lifecycle -----------------------------------------------------

bool GLRHIDevice::initSurface(const SurfaceDesc &desc)
{
    if (m_initialized)
        return true;

    // The window must have been set via constructor or externally before
    // calling initSurface. If a nativeWindow handle is provided but no
    // IWindow was set, we cannot proceed (GL context creation requires
    // the IWindow abstraction).
    if (!m_window)
    {
        LOG_ERROR(LogCategories::kEngine, "GLRHIDevice::initSurface: no IWindow associated");
        return false;
    }

    m_window->makeContextCurrent();

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        LOG_ERROR(LogCategories::kEngine, "GLAD init failed");
        return false;
    }

    LOG_INFO(LogCategories::kEngine, "OpenGL %s | GPU: %s", glGetString(GL_VERSION),
             glGetString(GL_RENDERER));

    m_settings.vsync = desc.vsync;
    glfwSwapInterval(m_settings.vsync ? 1 : 0);

    if (desc.width > 0 && desc.height > 0)
    {
        glViewport(0, 0, static_cast<GLsizei>(desc.width), static_cast<GLsizei>(desc.height));
    }
    else
    {
        int w, h;
        m_window->getSize(w, h);
        glViewport(0, 0, w, h);
    }

    m_initialized = true;
    LOG_INFO(LogCategories::kEngine, "GLRHIDevice surface initialized");
    return true;
}

void GLRHIDevice::shutdownSurface()
{
    // Surface teardown is lightweight for OpenGL — the context is owned
    // by the window. Full resource cleanup happens in shutdown().
    m_window = nullptr;
}

void GLRHIDevice::beginFrame()
{
    if (!m_window)
        return;
    int width, height;
    m_window->getSize(width, height);
    glViewport(0, 0, width, height);
}

void GLRHIDevice::endFrame()
{
    if (m_window)
    {
        m_window->swapBuffers();
    }
    signalFrame();
}

void GLRHIDevice::setClearColor(f32 r, f32 g, f32 b, f32 a)
{
    m_settings.clearColor[0] = r;
    m_settings.clearColor[1] = g;
    m_settings.clearColor[2] = b;
    m_settings.clearColor[3] = a;
}

void GLRHIDevice::clear(ClearFlags flags)
{
    GLbitfield mask = 0;
    if (hasFlag(flags, ClearFlags::Color))
    {
        glClearColor(m_settings.clearColor[0], m_settings.clearColor[1],
                     m_settings.clearColor[2], m_settings.clearColor[3]);
        mask |= GL_COLOR_BUFFER_BIT;
    }
    if (hasFlag(flags, ClearFlags::Depth))
        mask |= GL_DEPTH_BUFFER_BIT;
    if (hasFlag(flags, ClearFlags::Stencil))
        mask |= GL_STENCIL_BUFFER_BIT;
    if (mask)
        glClear(mask);
}

void GLRHIDevice::resizeSurface(u32 width, u32 height)
{
    glViewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
}

bool GLRHIDevice::readbackBackbuffer(std::vector<u8> &outPixelsRGBA8, u32 &outWidth, u32 &outHeight)
{
    // Read the default framebuffer's back buffer. Must run on the render
    // thread with the GL context current, after all draws, before present.
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glReadBuffer(GL_BACK);

    GLint vp[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_VIEWPORT, vp);
    if (vp[2] <= 0 || vp[3] <= 0)
        return false;

    const u32 width = static_cast<u32>(vp[2]);
    const u32 height = static_cast<u32>(vp[3]);
    outPixelsRGBA8.resize(static_cast<size_t>(width) * height * 4);

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height), GL_RGBA, GL_UNSIGNED_BYTE,
                 outPixelsRGBA8.data());

    // GL returns rows bottom-up; flip to top-left origin in place.
    const size_t rowBytes = static_cast<size_t>(width) * 4;
    std::vector<u8> row(rowBytes);
    u8 *data = outPixelsRGBA8.data();
    for (u32 y = 0; y < height / 2; ++y)
    {
        u8 *top = data + y * rowBytes;
        u8 *bottom = data + (height - 1 - y) * rowBytes;
        std::memcpy(row.data(), top, rowBytes);
        std::memcpy(top, bottom, rowBytes);
        std::memcpy(bottom, row.data(), rowBytes);
    }

    outWidth = width;
    outHeight = height;
    return true;
}

RHIFenceValue GLRHIDevice::signalFrame()
{
    GLsync sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    if (!sync)
    {
        // Fence creation failed; fall back to a CPU-side frame value that
        // will only advance when explicitly polled. This is safe but may
        // delay deletes until the queue is flushed at shutdown.
        return m_next_frame_value++;
    }

    const RHIFenceValue frame = m_next_frame_value++;
    FrameFence ff;
    ff.frame = frame;
    ff.sync = sync;
    m_frame_fences.pushBack(ff);
    return frame;
}

RHIFenceValue GLRHIDevice::getCompletedFenceValue()
{
    // Check outstanding fences in order; stop at the first unsignaled fence.
    while (!m_frame_fences.empty())
    {
        FrameFence &front = m_frame_fences.front();
        if (!front.sync)
        {
            // Fallback CPU frame: treat as immediately completed.
            m_completed_frame_value = front.frame;
            m_frame_fences.removeAt(0);
            continue;
        }

        const GLenum result = glClientWaitSync(front.sync, 0, 0);
        if (result == GL_ALREADY_SIGNALED || result == GL_CONDITION_SATISFIED)
        {
            m_completed_frame_value = front.frame;
            glDeleteSync(front.sync);
            m_frame_fences.removeAt(0);
        }
        else
        {
            break;
        }
    }
    return m_completed_frame_value;
}

void GLRHIDevice::queueResourceForDelete(GPUResource *resource)
{
    if (!resource)
        return;

    // Record the frame value the GPU must complete before destruction.
    // If no fence has been inserted yet, signal one now so we have a value.
    RHIFenceValue fence = m_next_frame_value;
    if (m_frame_fences.empty())
    {
        fence = signalFrame();
    }
    resource->setDeletionFence(fence);

    PendingDelete pd;
    pd.resource = resource;
    pd.fence = fence;
    m_pending_deletes.pushBack(pd);
}

void GLRHIDevice::flushPendingDeletes()
{
    const RHIFenceValue safe_frame = getCompletedFenceValue();

    usize keep = 0;
    for (usize i = 0; i < m_pending_deletes.size(); ++i)
    {
        PendingDelete &pd = m_pending_deletes[i];
        if (pd.fence <= safe_frame)
        {
            trackResourceDestroyed(pd.resource);
            pd.resource->internalDestroy();
        }
        else
        {
            if (keep != i)
            {
                m_pending_deletes[keep] = pd;
            }
            ++keep;
        }
    }

    if (keep != m_pending_deletes.size())
    {
        DynamicArray<PendingDelete> compacted;
        compacted.reserve(keep);
        for (usize i = 0; i < keep; ++i)
        {
            compacted.pushBack(m_pending_deletes[i]);
        }
        m_pending_deletes.swap(compacted);
    }
}

RHIMemoryInfo GLRHIDevice::queryMemoryInfo() const
{
    RHIMemoryInfo info{};

#if defined(GLAD_GL_NVX_gpu_memory_info)
    if (GLAD_GL_NVX_gpu_memory_info)
    {
        GLint total_kb = 0;
        GLint available_kb = 0;
        glGetIntegerv(GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX, &total_kb);
        glGetIntegerv(GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX, &available_kb);
        info.totalBytes = static_cast<u64>(total_kb) * 1024;
        info.availableBytes = static_cast<u64>(available_kb) * 1024;
        info.budgetBytes = info.totalBytes;
    }
#elif defined(GLAD_GL_ATI_meminfo)
    if (GLAD_GL_ATI_meminfo)
    {
        // Return the free memory in the first pool (best approximation).
        GLint values[4] = {0, 0, 0, 0};
        glGetIntegerv(GL_TEXTURE_FREE_MEMORY_ATI, values);
        info.availableBytes = static_cast<u64>(values[0]) * 1024;
    }
#endif

    return info;
}

// ==================================================================
// PSOManager
// ==================================================================

RHIPipelineStateRef PSOManager::getOrCreate(const PipelineStateDesc &desc, IRHIDevice *device)
{
    if (!device)
        return nullptr;

    if (RHIPipelineStateRef existing = find(desc))
        return existing;

    auto pso = device->createPipelineState(desc);
    // GLRHIDevice::createPipelineState already routes through this cache, in
    // which case the entry exists by now; only insert for devices that do
    // not self-cache.
    if (pso && !contains(desc))
        m_cache.insert(desc, pso);
    return pso;
}

bool PSOManager::contains(const PipelineStateDesc &desc) const
{
    return m_cache.contains(desc);
}

RHIPipelineStateRef PSOManager::find(const PipelineStateDesc &desc) const
{
    if (const auto *existing = m_cache.find(desc))
        return *existing;
    return nullptr;
}

void PSOManager::insert(const PipelineStateDesc &desc, RHIPipelineStateRef pso)
{
    m_cache.insert(desc, pso);
}

void PSOManager::clear()
{
    m_cache.clear();
}

usize PSOManager::getCacheSize() const
{
    return m_cache.size();
}

} // namespace Entelechy
