#include "draw_utils.h"

#include <iostream>
#include <cstring>
#include <vector>

#include <stdarg.h>

#define THROW_PROGRAM_CREATE(mess) throw GLProgram::CreateException(mess, __FILE__, __LINE__)
#define THROW_PIPELINE_VALIDATE(mess) throw GLPipeline::ValidateException(mess, __FILE__, __LINE__)

namespace {
    const GLuint INVALID_GL_OBJECT = static_cast<GLuint>(-1);

}

GLProgram::GLProgram(GLuint program, GLuint stage) : m_program(program), m_stage(stage)
{

}
GLProgram::~GLProgram()
{
}

std::shared_ptr<GLProgram> GLProgram::create_program(GLenum stage, GLsizei count, ...)
{
    class ScopedShader {
    private:
        GLuint shader;
    public:
        ScopedShader(GLenum stage) : shader(glCreateShader(stage))
        { }

        ~ScopedShader() { glDeleteShader(shader); }

        inline operator GLuint() { return shader; }
    };

    GLuint program = INVALID_GL_OBJECT;
    std::vector<const char*> sources;
    sources.reserve(count);
    va_list va;
    va_start(va, count);
    for (GLsizei i = 0; i < count; ++i)
        sources.push_back(va_arg(va, const char*));
    va_end(va);
    // we don't use the builtin function to be able to print proper errors
    // per creation step
    ScopedShader shader(stage);
    if (shader)
    {
        glShaderSource(shader, count, sources.data(), 0);
        glCompileShader(shader);
        GLint status = GL_FALSE;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
        if (status == GL_TRUE)
        {
            program = glCreateProgram();
            if (program)
            {
                glProgramParameteri(program, GL_PROGRAM_SEPARABLE, GL_TRUE);
                glAttachShader(program, shader);
                glLinkProgram(program);
                glDetachShader(program, shader);
            }
            else
                THROW_PROGRAM_CREATE("Error calling glCreatProgram!");
        }
        else
        {
            GLsizei log_length = 0;
            glGetShaderInfoLog(shader, 0, &log_length, 0);
            if (log_length > 0)
            {
                std::vector<GLchar> log(log_length, '\0');
                glGetShaderInfoLog(shader, log_length, 0, log.data());
                THROW_PROGRAM_CREATE(log.data());
            }
            else
                THROW_PROGRAM_CREATE("Unkown error while compiling the shader!");
        }
    }
    else
        THROW_PROGRAM_CREATE("Error calling glCreateShader!");

    return std::shared_ptr<GLProgram>(new GLProgram(program, stage));
}

GLPipeline::GLPipeline()
{
}

GLPipeline::~GLPipeline()
{
}

std::shared_ptr<GLPipeline> GLPipeline::create_pipeline()
{
    return std::shared_ptr<GLPipeline>(new GLPipeline());
}

GLPipeline& GLPipeline::add_program(std::shared_ptr<GLProgram>& program)
{
    m_programs.push_back(program);
    return *this;
}

void GLPipeline::validate()
{
    glGenProgramPipelines(1, &m_pipeline);
    for (const auto& program : m_programs)
        glUseProgramStages(m_pipeline, program->get_stage(), program->get_program());
    glValidateProgramPipeline(m_pipeline);
    GLint status = GL_FALSE;
    glGetProgramPipelineiv(m_pipeline, GL_VALIDATE_STATUS, &status);
    if (status != GL_TRUE)
    {
        GLsizei log_length = 0;
        glGetProgramPipelineInfoLog(m_pipeline, 0, &log_length, 0);
        if (log_length > 0)
        {
            std::vector<GLchar> log(log_length, '\0');
            glGetProgramPipelineInfoLog(m_pipeline, log_length, 0, log.data());
            THROW_PIPELINE_VALIDATE(log.data());
        }
        THROW_PIPELINE_VALIDATE("Unknown error when validating the pipeline!");
    }
}

GLuint GLPipeline::get_filled_stages() const
{
    GLuint stages = 0;
    for (const auto& program : m_programs)
        stages = stages | program->get_stage();
    return stages;
}

std::shared_ptr<GLProgram> GLPipeline::get_program(GLuint stage) const
{
    for (const auto& program : m_programs)
    {
        if (program->get_stage() | stage)
            return program;
    }
    return m_programs.front();
}

GLPipeline::ScopedSet::ScopedSet(const GLPipeline& pipeline)
{
    glBindProgramPipeline(pipeline.m_pipeline);
}

GLPipeline::ScopedSet::~ScopedSet()
{
    glBindProgramPipeline(0);
}
