#include <GL/glew.h>

class ShaderProgram {
public:
    ShaderProgram();
    ~ShaderProgram();

    void release();
    void create(const char* vertex_code, const char* fragment_code);
    bool is_valid() const;
    GLuint get_program() const;

    class ScopedSet {
    public:
        ScopedSet(const ShaderProgram& _program);
        ~ScopedSet();
    private:
        GLuint program;
    };
private:
    GLuint program;
    GLuint vertex_shader;
    GLuint fragment_shader;
};