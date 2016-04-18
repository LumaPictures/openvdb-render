#include <GL/glew.h>
#include <vector>
#include <memory>
#include <exception>
#include <sstream>

class GLProgram{
private:
    GLProgram(GLuint program, GLuint stage);
public:
    ~GLProgram();

    class CreateException{
    private:
        std::string m_error;
    public:
        CreateException(const char* error, const char* file, int line)
        {
            std::stringstream ss; ss << "[" << file << ":" << line << "] " << error;
            m_error = ss.str();
        }

        const char* what() const
        {
            return m_error.c_str();
        }
    };

    static std::shared_ptr<GLProgram> create_program(GLenum stage, GLsizei count, ...);

    inline GLuint get_stage() const
    {
        return m_stage;
    }

    inline GLuint get_program() const
    {
        return m_program;
    }
private:
    GLuint m_program;
    GLuint m_stage;
};

class GLPipeline{
private:
    GLPipeline();
public:
    ~GLPipeline();

    class ValidateException{
    private:
        std::string m_error;
    public:
        ValidateException(const char* error, const char* file, int line)
        {
            std::stringstream ss; ss << "[" << file << ":" << line << "] " << error;
            m_error = ss.str();
        }

        const char* what() const
        {
            return m_error.c_str();
        }
    };

    static std::shared_ptr<GLPipeline> create_pipeline();

    GLPipeline& add_program(std::shared_ptr<GLProgram>& program);
    void validate();

    GLuint get_filled_stages() const;
    std::shared_ptr<GLProgram> get_program(GLuint stage) const;

    class ScopedSet {
    public:
        ScopedSet(const GLPipeline& pipeline);
        ~ScopedSet();
    };
private:
    GLuint m_pipeline;
    std::vector<std::shared_ptr<GLProgram>> m_programs;
};
