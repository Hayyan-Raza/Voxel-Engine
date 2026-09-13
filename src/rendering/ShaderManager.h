#pragma once

#include <glad/gl.h>
#include <string>
#include <iostream>

class ShaderManager {
public:
    static GLuint LoadShader(const std::string& vertexPath, const std::string& fragmentPath);
    
private:
    static std::string ReadFile(const std::string& filepath);
    static GLuint CompileShader(GLenum type, const std::string& source, const std::string& filepath);
};
