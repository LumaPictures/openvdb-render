#include "draw_utils.h"

#include <iostream>
#include <cstring>
#include <vector>

namespace {
    const GLuint INVALID_GL_OBJECT = static_cast<GLuint>(-1);

    template <GLint shader_type>
    bool create_shader(GLuint& shader, const char* shader_code)
    {
        shader = glCreateShader(shader_type);

        GLint code_size = static_cast<GLint>(strlen(shader_code));
        glShaderSource(shader, 1, &shader_code, &code_size);
        glCompileShader(shader);
        GLint success = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (success == GL_FALSE)
        {
            std::cerr << "[openvdb_render] Error building shader : " << std::endl;
            std::cerr << shader_code << std::endl;
            GLint max_length = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &max_length);
            std::vector<GLchar> info_log(max_length);
            glGetShaderInfoLog(shader, max_length, &max_length, &info_log[0]);
            std::cerr << &info_log[0] << std::endl;
            return false;
        }
        else
            return true;
    }
}

ShaderProgram::ShaderProgram() : program(INVALID_GL_OBJECT), vertex_shader(INVALID_GL_OBJECT), fragment_shader(INVALID_GL_OBJECT)
{

}

ShaderProgram::~ShaderProgram()
{
    release();
}

void ShaderProgram::release()
{
    if (program != INVALID_GL_OBJECT)
        glDeleteProgram(program);
    program = INVALID_GL_OBJECT;
    if (vertex_shader != INVALID_GL_OBJECT)
        glDeleteShader(vertex_shader);
    vertex_shader = INVALID_GL_OBJECT;
    if (fragment_shader != INVALID_GL_OBJECT)
        glDeleteShader(fragment_shader);
    fragment_shader = INVALID_GL_OBJECT;
}

void ShaderProgram::create(const char* vertex_code, const char* fragment_code)
{
    if (!create_shader<GL_VERTEX_SHADER>(vertex_shader, vertex_code))
    {
        release();
        return;
    }
    if (!create_shader<GL_FRAGMENT_SHADER>(fragment_shader, fragment_code))
    {
        release();
        return;
    }

    program = glCreateProgram();

    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

    GLint success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (success == GL_FALSE)
    {
        std::cerr << "[openvdb_render] Error linking shader." << std::endl;

        GLint max_length = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &max_length);
        std::vector<GLchar> info_log(max_length);
        glGetProgramInfoLog(program, max_length, &max_length, &info_log[0]);

        std::cerr << &info_log[0] << std::endl;

        release();
        return;
    }

    glDetachShader(program, vertex_shader);
    glDetachShader(program, fragment_shader);
    return;
}

bool ShaderProgram::is_valid() const
{
    return program != INVALID_GL_OBJECT;
}

GLuint ShaderProgram::get_program() const
{
    return program;
}

ShaderProgram::ScopedSet::ScopedSet(const ShaderProgram& _program) : program(_program.program)
{
    glUseProgram(program);
}

ShaderProgram::ScopedSet::~ScopedSet()
{
    glUseProgram(0);
}
