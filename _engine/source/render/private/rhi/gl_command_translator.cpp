#include "render/rhi/gl_command_translator.h"
#include "render/rhi/gl_rhi_device.h"
#include "core/string/string_intern_pool.h"
#include <cstring>

namespace Entelechy
{

// ------------------------------------------------------------------
// GL helper functions (same as in the old GLCommandList)
// ------------------------------------------------------------------
namespace
{

GLenum getGLCullMode(CullMode mode)
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

GLenum getGLFrontFace(FrontFace face)
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

GLenum getGLCompareFunc(CompareFunc fn)
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

GLenum getGLBlendFactor(BlendFactor f)
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

GLenum getGLBlendOp(BlendOp op)
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

} // namespace

// ------------------------------------------------------------------
// State management
// ------------------------------------------------------------------
void GLCommandTranslator::resetState()
{
    m_bound_program = 0;
    m_bound_vao = 0;
    m_bound_ebo = 0;
    m_ebo_offset = 0;
    m_uniform_cache.clear();
    m_debug_group_depth = 0;
}

GLint GLCommandTranslator::getUniformLocation(StringId name)
{
    if (!m_bound_program || name.value() == 0)
        return -1;
    TranslatorUniformLocKey key{m_bound_program, name};
    if (auto *cached = m_uniform_cache.find(key))
    {
        return *cached;
    }
    const char *resolved = StringInternPool::instance().resolve(name);
    GLint loc = glGetUniformLocation(m_bound_program, resolved ? resolved : "");
    m_uniform_cache.insert(key, loc);
    return loc;
}

// ------------------------------------------------------------------
// Main replay loop
// ------------------------------------------------------------------
void GLCommandTranslator::execute(const RenderCommandBuffer &buffer)
{
    resetState();

    const u8 *ptr = buffer.data();
    const u8 *end = ptr + buffer.size();

    while (ptr < end)
    {
        const auto *header = reinterpret_cast<const RenderCmdHeader *>(ptr);
        const u8 *payloadPtr = ptr + sizeof(RenderCmdHeader);

        switch (header->type)
        {
        case RenderCommandType::BeginRenderPass:
        {
            const auto *cmd = reinterpret_cast<const CmdBeginRenderPass *>(payloadPtr);
            for (u32 i = 0; i < cmd->desc.colorAttachmentCount; ++i)
            {
                if (cmd->desc.colorAttachments[i].clear)
                {
                    const f32 *c = cmd->desc.colorAttachments[i].clearColor;
                    glClearColor(c[0], c[1], c[2], c[3]);
                    glClear(GL_COLOR_BUFFER_BIT);
                }
            }
            break;
        }

        case RenderCommandType::EndRenderPass:
            // No-op for GL (no explicit render pass end)
            break;

        case RenderCommandType::SetViewport:
        {
            const auto *cmd = reinterpret_cast<const CmdSetViewport *>(payloadPtr);
            glViewport(static_cast<GLint>(cmd->x), static_cast<GLint>(cmd->y),
                       static_cast<GLsizei>(cmd->w), static_cast<GLsizei>(cmd->h));
            break;
        }

        case RenderCommandType::SetScissor:
        {
            const auto *cmd = reinterpret_cast<const CmdSetScissor *>(payloadPtr);
            glEnable(GL_SCISSOR_TEST);
            glScissor(static_cast<GLint>(cmd->x), static_cast<GLint>(cmd->y),
                      static_cast<GLsizei>(cmd->w), static_cast<GLsizei>(cmd->h));
            break;
        }

        case RenderCommandType::BindPipeline:
        {
            const auto *cmd = reinterpret_cast<const CmdBindPipeline *>(payloadPtr);
            if (cmd->pso)
            {
                auto *glPso = static_cast<GLPipelineState *>(cmd->pso);
                m_bound_program = glPso->getProgram();
                glUseProgram(m_bound_program);

                // Apply rasterizer state
                const auto &raster = glPso->getDesc().rasterizerState;
                if (raster.cullMode != CullMode::None)
                {
                    glEnable(GL_CULL_FACE);
                    glCullFace(getGLCullMode(raster.cullMode));
                    glFrontFace(getGLFrontFace(raster.frontFace));
                }
                else
                {
                    glDisable(GL_CULL_FACE);
                }

                // Apply depth state
                const auto &ds = glPso->getDesc().depthStencilState;
                if (ds.depthTest)
                {
                    glEnable(GL_DEPTH_TEST);
                    glDepthFunc(getGLCompareFunc(ds.depthFunc));
                    glDepthMask(ds.depthWrite ? GL_TRUE : GL_FALSE);
                }
                else
                {
                    glDisable(GL_DEPTH_TEST);
                }

                // Apply blend state
                const auto &blend = glPso->getDesc().blendState;
                if (blend.enable)
                {
                    glEnable(GL_BLEND);
                    glBlendFunc(getGLBlendFactor(blend.srcFactor), getGLBlendFactor(blend.dstFactor));
                    glBlendEquation(getGLBlendOp(blend.op));
                }
                else
                {
                    glDisable(GL_BLEND);
                }
            }
            break;
        }

        case RenderCommandType::BindVertexBuffer:
        {
            const auto *cmd = reinterpret_cast<const CmdBindVertexBuffer *>(payloadPtr);
            if (cmd->buffer)
            {
                auto *glBuf = static_cast<GLBuffer *>(cmd->buffer);
                if (glBuf->getVAO())
                {
                    m_bound_vao = glBuf->getVAO();
                    glBindVertexArray(m_bound_vao);
                }
                else
                {
                    m_bound_vao = 0;
                    glBindBuffer(GL_ARRAY_BUFFER, glBuf->getVBO());
                }
            }
            break;
        }

        case RenderCommandType::BindIndexBuffer:
        {
            const auto *cmd = reinterpret_cast<const CmdBindIndexBuffer *>(payloadPtr);
            if (cmd->buffer)
            {
                auto *glBuf = static_cast<GLBuffer *>(cmd->buffer);
                m_bound_ebo = glBuf->getVBO();
                m_ebo_offset = cmd->offset;
            }
            break;
        }

        case RenderCommandType::DrawIndexed:
        {
            const auto *cmd = reinterpret_cast<const CmdDrawIndexed *>(payloadPtr);
            if (m_bound_vao && m_bound_ebo)
            {
                glBindVertexArray(m_bound_vao);
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_bound_ebo);
            }

            GLenum topology = GL_TRIANGLES;
            GLsizei count = static_cast<GLsizei>(cmd->indexCount);
            GLenum indexType = GL_UNSIGNED_INT;
            const void *indexOffset = reinterpret_cast<const void *>(
                static_cast<uintptr_t>(m_ebo_offset) + cmd->startIndex * sizeof(u32));

            if (cmd->baseVertex != 0)
            {
                glDrawElementsBaseVertex(topology, count, indexType, indexOffset, cmd->baseVertex);
            }
            else
            {
                glDrawElements(topology, count, indexType, indexOffset);
            }
            break;
        }

        case RenderCommandType::Draw:
        {
            const auto *cmd = reinterpret_cast<const CmdDraw *>(payloadPtr);
            if (m_bound_vao)
            {
                glBindVertexArray(m_bound_vao);
            }
            glDrawArrays(GL_TRIANGLES, static_cast<GLint>(cmd->startVertex),
                         static_cast<GLsizei>(cmd->vertexCount));
            break;
        }

        case RenderCommandType::ResourceBarrier:
            // No-op on OpenGL (driver handles implicit barriers)
            break;

        case RenderCommandType::ClearRenderTarget:
        {
            const auto *cmd = reinterpret_cast<const CmdClearRenderTarget *>(payloadPtr);
            glClearColor(cmd->color[0], cmd->color[1], cmd->color[2], cmd->color[3]);
            glClear(GL_COLOR_BUFFER_BIT);
            break;
        }

        case RenderCommandType::SetUniformFloat:
        {
            const auto *cmd = reinterpret_cast<const CmdSetUniformFloat *>(payloadPtr);
            GLint loc = getUniformLocation(cmd->name);
            if (loc >= 0)
                glUniform1f(loc, cmd->value);
            break;
        }

        case RenderCommandType::SetUniformInt:
        {
            const auto *cmd = reinterpret_cast<const CmdSetUniformInt *>(payloadPtr);
            GLint loc = getUniformLocation(cmd->name);
            if (loc >= 0)
                glUniform1i(loc, cmd->value);
            break;
        }

        case RenderCommandType::SetUniformVec2:
        {
            const auto *cmd = reinterpret_cast<const CmdSetUniformVec2 *>(payloadPtr);
            GLint loc = getUniformLocation(cmd->name);
            if (loc >= 0)
                glUniform2fv(loc, 1, cmd->value);
            break;
        }

        case RenderCommandType::SetUniformVec3:
        {
            const auto *cmd = reinterpret_cast<const CmdSetUniformVec3 *>(payloadPtr);
            GLint loc = getUniformLocation(cmd->name);
            if (loc >= 0)
                glUniform3fv(loc, 1, cmd->value);
            break;
        }

        case RenderCommandType::SetUniformVec4:
        {
            const auto *cmd = reinterpret_cast<const CmdSetUniformVec4 *>(payloadPtr);
            GLint loc = getUniformLocation(cmd->name);
            if (loc >= 0)
                glUniform4fv(loc, 1, cmd->value);
            break;
        }

        case RenderCommandType::SetUniformMat3:
        {
            const auto *cmd = reinterpret_cast<const CmdSetUniformMat3 *>(payloadPtr);
            GLint loc = getUniformLocation(cmd->name);
            if (loc >= 0)
                glUniformMatrix3fv(loc, 1, cmd->transpose ? GL_TRUE : GL_FALSE, cmd->value);
            break;
        }

        case RenderCommandType::SetUniformMat4:
        {
            const auto *cmd = reinterpret_cast<const CmdSetUniformMat4 *>(payloadPtr);
            GLint loc = getUniformLocation(cmd->name);
            if (loc >= 0)
                glUniformMatrix4fv(loc, 1, cmd->transpose ? GL_TRUE : GL_FALSE, cmd->value);
            break;
        }

        case RenderCommandType::BindTexture:
        {
            const auto *cmd = reinterpret_cast<const CmdBindTexture *>(payloadPtr);
            if (cmd->texture)
            {
                auto *glTex = static_cast<GLTexture *>(cmd->texture);
                glActiveTexture(GL_TEXTURE0 + cmd->slot);
                glBindTexture(glTex->getTarget(), glTex->getTexture());
            }
            break;
        }

        case RenderCommandType::BindConstantBuffer:
            // No-op on OpenGL for now. When UBO/CBV unified binding lands,
            // this will translate to glBindBufferBase(GL_UNIFORM_BUFFER, ...).
            break;

        case RenderCommandType::SetPushConstants:
            // No-op on OpenGL. Push constants map to root constants (D3D12) or
            // vkCmdPushConstants (Vulkan). GL uses glUniform* instead.
            break;

        case RenderCommandType::PushDebugGroup:
        {
            const auto *cmd = reinterpret_cast<const CmdDebugString *>(payloadPtr);
            if (cmd->nameLength > 0)
            {
                const char *name = reinterpret_cast<const char *>(
                    payloadPtr + sizeof(CmdDebugString));
#if defined(GLAD_GL_KHR_debug)
                glPushDebugGroupKHR(GL_DEBUG_SOURCE_APPLICATION_KHR, 0, -1, name);
#elif defined(GLAD_GL_VERSION_4_3)
                glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, name);
#endif
            }
            ++m_debug_group_depth;
            break;
        }

        case RenderCommandType::PopDebugGroup:
        {
#if defined(GLAD_GL_KHR_debug)
            if (m_debug_group_depth > 0)
            {
                glPopDebugGroupKHR();
            }
#elif defined(GLAD_GL_VERSION_4_3)
            if (m_debug_group_depth > 0)
            {
                glPopDebugGroup();
            }
#endif
            if (m_debug_group_depth > 0)
            {
                --m_debug_group_depth;
            }
            break;
        }

        case RenderCommandType::InsertDebugMarker:
        {
            const auto *cmd = reinterpret_cast<const CmdDebugString *>(payloadPtr);
            if (cmd->nameLength > 0)
            {
                const char *name = reinterpret_cast<const char *>(
                    payloadPtr + sizeof(CmdDebugString));
#if defined(GLAD_GL_KHR_debug)
                glDebugMessageInsertKHR(GL_DEBUG_SOURCE_APPLICATION_KHR, GL_DEBUG_TYPE_MARKER_KHR, 0,
                                        GL_DEBUG_SEVERITY_NOTIFICATION_KHR, -1, name);
#elif defined(GLAD_GL_VERSION_4_3)
                glDebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_MARKER, 0,
                                     GL_DEBUG_SEVERITY_NOTIFICATION, -1, name);
#endif
            }
            break;
        }
        }

        // Advance to next command
        usize stride = sizeof(RenderCmdHeader) + AlignUp(header->payloadSize, alignof(std::max_align_t));
        ptr += stride;
    }
}

} // namespace Entelechy
