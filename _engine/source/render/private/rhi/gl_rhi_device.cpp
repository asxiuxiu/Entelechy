#include "render/rhi/gl_rhi_device.h"
#include "log/core/log_macros.h"
#include "core/allocator/allocator.h"
#include <cstring>
#include <memory>

namespace Entelechy {

namespace {
    template<typename T, typename... Args>
    T* allocateResource(Args&&... args) {
        void* mem = DefaultAllocator::alloc(sizeof(T), alignof(T));
        return new (mem) T(std::forward<Args>(args)...);
    }
} // namespace

// ==================================================================
// Helper: TextureFormat -> OpenGL format mapping
// ==================================================================
struct GLFormatInfo {
    GLint internalFormat;
    GLenum format;
    GLenum type;
};

static GLFormatInfo getGLFormatInfo(TextureFormat fmt) {
    switch (fmt) {
        case TextureFormat::R8_UNORM:           return {GL_R8,               GL_RED,             GL_UNSIGNED_BYTE};
        case TextureFormat::RG8_UNORM:          return {GL_RG8,              GL_RG,              GL_UNSIGNED_BYTE};
        case TextureFormat::RGBA8_UNORM:        return {GL_RGBA8,            GL_RGBA,            GL_UNSIGNED_BYTE};
        case TextureFormat::RGBA8_SRGB:         return {GL_SRGB8_ALPHA8,     GL_RGBA,            GL_UNSIGNED_BYTE};
        case TextureFormat::R16_FLOAT:          return {GL_R16F,             GL_RED,             GL_HALF_FLOAT};
        case TextureFormat::RG16_FLOAT:         return {GL_RG16F,            GL_RG,              GL_HALF_FLOAT};
        case TextureFormat::RGBA16_FLOAT:       return {GL_RGBA16F,          GL_RGBA,            GL_HALF_FLOAT};
        case TextureFormat::R32_FLOAT:          return {GL_R32F,             GL_RED,             GL_FLOAT};
        case TextureFormat::RG32_FLOAT:         return {GL_RG32F,            GL_RG,              GL_FLOAT};
        case TextureFormat::RGBA32_FLOAT:       return {GL_RGBA32F,          GL_RGBA,            GL_FLOAT};
        case TextureFormat::D24_UNORM_S8_UINT:  return {GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL,   GL_UNSIGNED_INT_24_8};
        case TextureFormat::D32_FLOAT:          return {GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT};
        default:                                return {GL_RGBA8,            GL_RGBA,            GL_UNSIGNED_BYTE};
    }
}

static GLenum getGLTopology(PrimitiveTopology topo) {
    switch (topo) {
        case PrimitiveTopology::Triangles:      return GL_TRIANGLES;
        case PrimitiveTopology::TriangleStrip:  return GL_TRIANGLE_STRIP;
        case PrimitiveTopology::Lines:          return GL_LINES;
        case PrimitiveTopology::LineStrip:      return GL_LINE_STRIP;
        case PrimitiveTopology::Points:         return GL_POINTS;
        default:                                return GL_TRIANGLES;
    }
}

static GLenum getGLCullMode(CullMode mode) {
    switch (mode) {
        case CullMode::None:  return GL_NONE;
        case CullMode::Front: return GL_FRONT;
        case CullMode::Back:  return GL_BACK;
        default:              return GL_BACK;
    }
}

static GLenum getGLFrontFace(FrontFace face) {
    switch (face) {
        case FrontFace::CounterClockwise: return GL_CCW;
        case FrontFace::Clockwise:        return GL_CW;
        default:                          return GL_CCW;
    }
}

static GLenum getGLCompareFunc(CompareFunc fn) {
    switch (fn) {
        case CompareFunc::Never:        return GL_NEVER;
        case CompareFunc::Less:         return GL_LESS;
        case CompareFunc::Equal:        return GL_EQUAL;
        case CompareFunc::LessEqual:    return GL_LEQUAL;
        case CompareFunc::Greater:      return GL_GREATER;
        case CompareFunc::NotEqual:     return GL_NOTEQUAL;
        case CompareFunc::GreaterEqual: return GL_GEQUAL;
        case CompareFunc::Always:       return GL_ALWAYS;
        default:                        return GL_LESS;
    }
}

static GLenum getGLBlendFactor(BlendFactor f) {
    switch (f) {
        case BlendFactor::Zero:                 return GL_ZERO;
        case BlendFactor::One:                  return GL_ONE;
        case BlendFactor::SrcColor:             return GL_SRC_COLOR;
        case BlendFactor::OneMinusSrcColor:     return GL_ONE_MINUS_SRC_COLOR;
        case BlendFactor::SrcAlpha:             return GL_SRC_ALPHA;
        case BlendFactor::OneMinusSrcAlpha:     return GL_ONE_MINUS_SRC_ALPHA;
        case BlendFactor::DstColor:             return GL_DST_COLOR;
        case BlendFactor::OneMinusDstColor:     return GL_ONE_MINUS_DST_COLOR;
        case BlendFactor::DstAlpha:             return GL_DST_ALPHA;
        case BlendFactor::OneMinusDstAlpha:     return GL_ONE_MINUS_DST_ALPHA;
        default:                                return GL_ONE;
    }
}

static GLenum getGLBlendOp(BlendOp op) {
    switch (op) {
        case BlendOp::Add:              return GL_FUNC_ADD;
        case BlendOp::Subtract:         return GL_FUNC_SUBTRACT;
        case BlendOp::ReverseSubtract:  return GL_FUNC_REVERSE_SUBTRACT;
        case BlendOp::Min:              return GL_MIN;
        case BlendOp::Max:              return GL_MAX;
        default:                        return GL_FUNC_ADD;
    }
}

static GLenum getGLShaderStage(ShaderStage stage) {
    switch (stage) {
        case ShaderStage::Vertex:   return GL_VERTEX_SHADER;
        case ShaderStage::Fragment: return GL_FRAGMENT_SHADER;
        case ShaderStage::Geometry: return GL_GEOMETRY_SHADER;
        default:                    return GL_VERTEX_SHADER;
    }
}

// ==================================================================
// GLBuffer
// ==================================================================
GLBuffer::GLBuffer(u32 size, BufferUsage usage, GLuint vbo, GLuint vao)
    : m_size(size), m_usage(usage), m_vbo(vbo), m_vao(vao) {
}

GLBuffer::~GLBuffer() {
    onDestroy();
}

void GLBuffer::onDestroy() {
    if (m_vao) {
        glDeleteVertexArrays(1, &m_vao);
        m_vao = 0;
    }
    if (m_vbo) {
        glDeleteBuffers(1, &m_vbo);
        m_vbo = 0;
    }
}

void GLBuffer::setDebugName(const SmallString& name) {
#if defined(GLAD_GL_KHR_debug)
    if (m_vbo && !name.empty()) {
        glObjectLabelKHR(GL_BUFFER_KHR, m_vbo, static_cast<GLsizei>(name.length()), name.c_str());
    }
#elif defined(GLAD_GL_VERSION_4_3)
    if (m_vbo && !name.empty()) {
        glObjectLabel(GL_BUFFER, m_vbo, static_cast<GLsizei>(name.length()), name.c_str());
    }
#endif
}

// ==================================================================
// GLTexture
// ==================================================================
GLTexture::GLTexture(const TextureDesc& desc, GLuint texture, GLenum target)
    : m_desc(desc), m_texture(texture), m_target(target) {
}

GLTexture::~GLTexture() {
    onDestroy();
}

void GLTexture::onDestroy() {
    if (m_texture) {
        glDeleteTextures(1, &m_texture);
        m_texture = 0;
    }
}

void GLTexture::setDebugName(const SmallString& name) {
#if defined(GLAD_GL_KHR_debug)
    if (m_texture && !name.empty()) {
        glObjectLabelKHR(GL_TEXTURE_KHR, m_texture, static_cast<GLsizei>(name.length()), name.c_str());
    }
#elif defined(GLAD_GL_VERSION_4_3)
    if (m_texture && !name.empty()) {
        glObjectLabel(GL_TEXTURE, m_texture, static_cast<GLsizei>(name.length()), name.c_str());
    }
#endif
}

// ==================================================================
// GLShader
// ==================================================================
GLShader::GLShader(ShaderStage stage, GLuint shader)
    : m_stage(stage), m_shader(shader) {
}

GLShader::~GLShader() {
    onDestroy();
}

void GLShader::onDestroy() {
    if (m_shader) {
        glDeleteShader(m_shader);
        m_shader = 0;
    }
}

void GLShader::setDebugName(const SmallString& name) {
#if defined(GLAD_GL_KHR_debug)
    if (m_shader && !name.empty()) {
        glObjectLabelKHR(GL_SHADER_KHR, m_shader, static_cast<GLsizei>(name.length()), name.c_str());
    }
#elif defined(GLAD_GL_VERSION_4_3)
    if (m_shader && !name.empty()) {
        glObjectLabel(GL_SHADER, m_shader, static_cast<GLsizei>(name.length()), name.c_str());
    }
#endif
}

// ==================================================================
// GLPipelineState
// ==================================================================
GLPipelineState::GLPipelineState(const PipelineStateDesc& desc, GLuint program)
    : m_desc(desc), m_program(program) {
}

GLPipelineState::~GLPipelineState() {
    onDestroy();
}

void GLPipelineState::onDestroy() {
    if (m_program) {
        glDeleteProgram(m_program);
        m_program = 0;
    }
}

void GLPipelineState::setDebugName(const SmallString& name) {
#if defined(GLAD_GL_KHR_debug)
    if (m_program && !name.empty()) {
        glObjectLabelKHR(GL_PROGRAM_KHR, m_program, static_cast<GLsizei>(name.length()), name.c_str());
    }
#elif defined(GLAD_GL_VERSION_4_3)
    if (m_program && !name.empty()) {
        glObjectLabel(GL_PROGRAM, m_program, static_cast<GLsizei>(name.length()), name.c_str());
    }
#endif
}

// ==================================================================
// GLCommandList
// ==================================================================

void GLCommandList::begin() {
    m_inside_render_pass = false;
    m_bound_program = 0;
    m_bound_vao = 0;
    m_bound_ebo = 0;
    m_ebo_offset = 0;
    m_uniform_cache.clear();
    m_debug_group_depth = 0;
}

// ------------------------------------------------------------------
// Uniform location cache
// ------------------------------------------------------------------
GLint GLCommandList::getUniformLocation(const char* name) {
    if (!m_bound_program || !name) return -1;
    StringId sid(name);
    UniformLocKey key{m_bound_program, sid};
    if (auto* cached = m_uniform_cache.find(key)) {
        return *cached;
    }
    GLint loc = glGetUniformLocation(m_bound_program, name);
    m_uniform_cache.insert(key, loc);
    return loc;
}

void GLCommandList::end() {
    if (m_inside_render_pass) {
        endRenderPass();
    }
}

void GLCommandList::beginRenderPass(const RenderPassDesc& desc) {
    if (m_inside_render_pass) {
        endRenderPass();
    }
    m_inside_render_pass = true;

    // For now, assume framebuffer 0 (default / swapchain)
    // In the future, bind FBO based on desc attachments
    for (u32 i = 0; i < desc.colorAttachmentCount; ++i) {
        if (desc.colorAttachments[i].clear) {
            const f32* c = desc.colorAttachments[i].clearColor;
            glClearColor(c[0], c[1], c[2], c[3]);
            glClear(GL_COLOR_BUFFER_BIT);
        }
    }
}

void GLCommandList::endRenderPass() {
    m_inside_render_pass = false;
}

void GLCommandList::setViewport(f32 x, f32 y, f32 w, f32 h) {
    glViewport(static_cast<GLint>(x), static_cast<GLint>(y),
               static_cast<GLsizei>(w), static_cast<GLsizei>(h));
}

void GLCommandList::setScissor(u32 x, u32 y, u32 w, u32 h) {
    glEnable(GL_SCISSOR_TEST);
    glScissor(static_cast<GLint>(x), static_cast<GLint>(y),
              static_cast<GLsizei>(w), static_cast<GLsizei>(h));
}

void GLCommandList::bindPipeline(RHIPipelineState* pso) {
    if (!pso) return;
    auto* glPso = static_cast<GLPipelineState*>(pso);
    m_bound_program = glPso->getProgram();
    glUseProgram(m_bound_program);

    // Apply rasterizer state
    const auto& raster = glPso->getDesc().rasterizerState;
    if (raster.cullMode != CullMode::None) {
        glEnable(GL_CULL_FACE);
        glCullFace(getGLCullMode(raster.cullMode));
        glFrontFace(getGLFrontFace(raster.frontFace));
    } else {
        glDisable(GL_CULL_FACE);
    }

    // Apply depth state
    const auto& ds = glPso->getDesc().depthStencilState;
    if (ds.depthTest) {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(getGLCompareFunc(ds.depthFunc));
        glDepthMask(ds.depthWrite ? GL_TRUE : GL_FALSE);
    } else {
        glDisable(GL_DEPTH_TEST);
    }

    // Apply blend state
    const auto& blend = glPso->getDesc().blendState;
    if (blend.enable) {
        glEnable(GL_BLEND);
        glBlendFunc(getGLBlendFactor(blend.srcFactor), getGLBlendFactor(blend.dstFactor));
        glBlendEquation(getGLBlendOp(blend.op));
    } else {
        glDisable(GL_BLEND);
    }
}

void GLCommandList::bindVertexBuffer(RHIBuffer* buffer, u32 /*slot*/, u32 /*offset*/) {
    if (!buffer) return;
    auto* glBuf = static_cast<GLBuffer*>(buffer);
    if (glBuf->getVAO()) {
        m_bound_vao = glBuf->getVAO();
        glBindVertexArray(m_bound_vao);
    } else {
        // Legacy mode: bind VBO only, no VAO
        m_bound_vao = 0;
        glBindBuffer(GL_ARRAY_BUFFER, glBuf->getVBO());
    }
}

void GLCommandList::bindIndexBuffer(RHIBuffer* buffer, u32 offset) {
    if (!buffer) return;
    auto* glBuf = static_cast<GLBuffer*>(buffer);
    m_bound_ebo = glBuf->getVBO(); // For index buffers, VBO field stores EBO name
    m_ebo_offset = offset;
}

void GLCommandList::drawIndexed(u32 indexCount, u32 startIndex, i32 baseVertex) {
    if (m_bound_vao && m_bound_ebo) {
        // Bind EBO to the current VAO
        glBindVertexArray(m_bound_vao);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_bound_ebo);
    }

    GLenum topology = GL_TRIANGLES; // TODO: track from bound pipeline
    GLsizei count = static_cast<GLsizei>(indexCount);
    GLenum indexType = GL_UNSIGNED_INT; // TODO: derive from buffer desc
    const void* indexOffset = reinterpret_cast<const void*>(
        static_cast<uintptr_t>(m_ebo_offset) + startIndex * sizeof(u32));

    if (baseVertex != 0) {
        glDrawElementsBaseVertex(topology, count, indexType, indexOffset, baseVertex);
    } else {
        glDrawElements(topology, count, indexType, indexOffset);
    }
}

void GLCommandList::draw(u32 vertexCount, u32 startVertex) {
    if (m_bound_vao) {
        glBindVertexArray(m_bound_vao);
    }
    glDrawArrays(GL_TRIANGLES, static_cast<GLint>(startVertex), static_cast<GLsizei>(vertexCount));
}

void GLCommandList::resourceBarrier(const BarrierDesc* /*barriers*/, u32 /*count*/) {
    // No-op on OpenGL backend (driver handles implicit barriers)
}

void GLCommandList::clearRenderTarget(u32 /*attachmentIndex*/, const f32 color[4]) {
    glClearColor(color[0], color[1], color[2], color[3]);
    glClear(GL_COLOR_BUFFER_BIT);
}

// ==================================================================
// GLRHIDevice
// ==================================================================

GLRHIDevice::GLRHIDevice() = default;

GLRHIDevice::~GLRHIDevice() {
    if (m_initialized) {
        shutdown();
    }
}

bool GLRHIDevice::initialize() {
    if (m_initialized) return true;
    m_initialized = true;
    LOG_INFO(LogCategories::kEngine, "GLRHIDevice initialized (OpenGL RHI)");
    return true;
}

void GLRHIDevice::shutdown() {
    m_pso_manager.clear();
    m_initialized = false;
}

RHIBufferRef GLRHIDevice::createBuffer(const BufferDesc& desc, const void* initialData) {
    GLuint bufferObj = 0;
    glGenBuffers(1, &bufferObj);
    if (!bufferObj) {
        LOG_ERROR(LogCategories::kEngine, "glGenBuffers failed");
        return nullptr;
    }

    GLenum target = GL_ARRAY_BUFFER;
    if (hasFlag(desc.usage, BufferUsage::Index)) {
        target = GL_ELEMENT_ARRAY_BUFFER;
    } else if (hasFlag(desc.usage, BufferUsage::Uniform)) {
        target = GL_UNIFORM_BUFFER;
    }

    glBindBuffer(target, bufferObj);
    glBufferData(target, desc.size, initialData, GL_STATIC_DRAW);
    glBindBuffer(target, 0);

    GLuint vao = 0;
    // If this is a vertex buffer with layout info, create and configure a VAO
    if (hasFlag(desc.usage, BufferUsage::Vertex) &&
        desc.vertexAttributes && desc.vertexAttributeCount > 0) {
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, bufferObj);

        for (u32 i = 0; i < desc.vertexAttributeCount; ++i) {
            const auto& attr = desc.vertexAttributes[i];
            glEnableVertexAttribArray(attr.location);
            glVertexAttribPointer(attr.location, static_cast<GLint>(attr.components),
                                  GL_FLOAT, attr.normalized ? GL_TRUE : GL_FALSE,
                                  static_cast<GLsizei>(desc.vertexStride),
                                  reinterpret_cast<const void*>(static_cast<uintptr_t>(attr.offset)));
        }

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    return RHIBufferRef(allocateResource<GLBuffer>(desc.size, desc.usage, bufferObj, vao));
}

RHITextureRef GLRHIDevice::createTexture(const TextureDesc& desc, const void* initialData) {
    GLuint tex = 0;
    glGenTextures(1, &tex);
    if (!tex) {
        LOG_ERROR(LogCategories::kEngine, "glGenTextures failed");
        return nullptr;
    }

    auto info = getGLFormatInfo(desc.format);

    if (desc.depth == 1) {
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, info.internalFormat,
                     static_cast<GLsizei>(desc.width), static_cast<GLsizei>(desc.height),
                     0, info.format, info.type, initialData);

        if (desc.mipLevels > 1) {
            glGenerateMipmap(GL_TEXTURE_2D);
        }

        // Default sampler params
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                        desc.mipLevels > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glBindTexture(GL_TEXTURE_2D, 0);
        return RHITextureRef(allocateResource<GLTexture>(desc, tex, GL_TEXTURE_2D));
    } else {
        // 3D texture (simplified path)
        glBindTexture(GL_TEXTURE_3D, tex);
        glTexImage3D(GL_TEXTURE_3D, 0, info.internalFormat,
                     static_cast<GLsizei>(desc.width), static_cast<GLsizei>(desc.height),
                     static_cast<GLsizei>(desc.depth),
                     0, info.format, info.type, initialData);
        glBindTexture(GL_TEXTURE_3D, 0);
        return RHITextureRef(allocateResource<GLTexture>(desc, tex, GL_TEXTURE_3D));
    }
}

RHIShaderRef GLRHIDevice::createShader(ShaderStage stage, const void* bytecode, size_t size) {
    GLenum glStage = getGLShaderStage(stage);
    GLuint shader = glCreateShader(glStage);
    if (!shader) {
        LOG_ERROR(LogCategories::kEngine, "glCreateShader failed");
        return nullptr;
    }

    const GLchar* source = static_cast<const GLchar*>(bytecode);
    GLint length = static_cast<GLint>(size);
    glShaderSource(shader, 1, &source, &length);
    glCompileShader(shader);

    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        LOG_ERROR(LogCategories::kEngine, "Shader compile error: %s", log);
        glDeleteShader(shader);
        return nullptr;
    }

    return RHIShaderRef(allocateResource<GLShader>(stage, shader));
}

RHIPipelineStateRef GLRHIDevice::createPipelineState(const PipelineStateDesc& desc) {
    GLuint program = glCreateProgram();
    if (!program) {
        LOG_ERROR(LogCategories::kEngine, "glCreateProgram failed");
        return nullptr;
    }

    if (desc.vertexShader) {
        auto* glVs = static_cast<GLShader*>(desc.vertexShader);
        glAttachShader(program, glVs->getShader());
    }
    if (desc.fragmentShader) {
        auto* glFs = static_cast<GLShader*>(desc.fragmentShader);
        glAttachShader(program, glFs->getShader());
    }

    glLinkProgram(program);

    GLint success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char log[512];
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        LOG_ERROR(LogCategories::kEngine, "Program link error: %s", log);
        glDeleteProgram(program);
        return nullptr;
    }

    return RHIPipelineStateRef(allocateResource<GLPipelineState>(desc, program));
}

IRHICommandList* GLRHIDevice::createCommandList() {
    // Phase 1: return the single internal command list
    m_cmd_list.begin();
    return &m_cmd_list;
}

void GLRHIDevice::submit(IRHICommandList* cmdList) {
    if (!cmdList) return;
    // Phase 1 (immediate GL): commands have already been executed.
    // In future phases this will translate and queue to GPU.
    cmdList->end();
}

void GLRHIDevice::present() {
    // Phase 1: SwapBuffers is handled by OpenGLBackend / Window layer.
    // Future: this may insert a present command into the queue.
}

// ==================================================================
// PSOManager
// ==================================================================

RHIPipelineStateRef PSOManager::getOrCreate(const PipelineStateDesc& desc, IRHIDevice* device) {
    if (!device) return nullptr;

    if (auto* existing = m_cache.find(desc)) {
        return *existing;
    }

    auto pso = device->createPipelineState(desc);
    if (pso) {
        m_cache.insert(desc, pso);
    }
    return pso;
}

bool PSOManager::contains(const PipelineStateDesc& desc) const {
    return m_cache.contains(desc);
}

void PSOManager::clear() {
    m_cache.clear();
}

usize PSOManager::getCacheSize() const {
    return m_cache.size();
}

// ==================================================================
// GLCommandList — Uniform and texture binding (Phase 1)
// ==================================================================

void GLCommandList::setUniformFloat(const char* name, f32 value) {
    GLint loc = getUniformLocation(name);
    if (loc >= 0) glUniform1f(loc, value);
}

void GLCommandList::setUniformInt(const char* name, i32 value) {
    GLint loc = getUniformLocation(name);
    if (loc >= 0) glUniform1i(loc, value);
}

void GLCommandList::setUniformVec2(const char* name, const f32* value) {
    if (!value) return;
    GLint loc = getUniformLocation(name);
    if (loc >= 0) glUniform2fv(loc, 1, value);
}

void GLCommandList::setUniformVec3(const char* name, const f32* value) {
    if (!value) return;
    GLint loc = getUniformLocation(name);
    if (loc >= 0) glUniform3fv(loc, 1, value);
}

void GLCommandList::setUniformVec4(const char* name, const f32* value) {
    if (!value) return;
    GLint loc = getUniformLocation(name);
    if (loc >= 0) glUniform4fv(loc, 1, value);
}

void GLCommandList::setUniformMat4(const char* name, const f32* value, bool transpose) {
    if (!value) return;
    GLint loc = getUniformLocation(name);
    if (loc >= 0) glUniformMatrix4fv(loc, 1, transpose ? GL_TRUE : GL_FALSE, value);
}

void GLCommandList::bindTexture(u32 slot, RHITexture* texture) {
    if (!texture) return;
    auto* glTex = static_cast<GLTexture*>(texture);
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(glTex->getTarget(), glTex->getTexture());
}

// ------------------------------------------------------------------
// Debug markers
// ------------------------------------------------------------------
void GLCommandList::pushDebugGroup(const char* name) {
#if defined(GLAD_GL_KHR_debug)
    if (name) {
        glPushDebugGroupKHR(GL_DEBUG_SOURCE_APPLICATION_KHR, 0, -1, name);
    }
#elif defined(GLAD_GL_VERSION_4_3)
    if (name) {
        glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, name);
    }
#endif
    ++m_debug_group_depth;
}

void GLCommandList::popDebugGroup() {
#if defined(GLAD_GL_KHR_debug)
    if (m_debug_group_depth > 0) {
        glPopDebugGroupKHR();
    }
#elif defined(GLAD_GL_VERSION_4_3)
    if (m_debug_group_depth > 0) {
        glPopDebugGroup();
    }
#endif
    if (m_debug_group_depth > 0) {
        --m_debug_group_depth;
    }
}

void GLCommandList::insertDebugMarker(const char* name) {
#if defined(GLAD_GL_KHR_debug)
    if (name) {
        glDebugMessageInsertKHR(GL_DEBUG_SOURCE_APPLICATION_KHR, GL_DEBUG_TYPE_MARKER_KHR,
                                0, GL_DEBUG_SEVERITY_NOTIFICATION_KHR, -1, name);
    }
#elif defined(GLAD_GL_VERSION_4_3)
    if (name) {
        glDebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_MARKER,
                             0, GL_DEBUG_SEVERITY_NOTIFICATION, -1, name);
    }
#endif
}

} // namespace Entelechy
