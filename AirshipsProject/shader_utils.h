#pragma once
#include <string>
#include <GL/glew.h>
#include <iostream>
#include <fstream>
#include <sstream>

GLuint CompileShader(GLenum type, const char* source);
std::string LoadShaderFromFile(const std::string& filename);
GLuint CreateShaderProgramFromFiles(const std::string& vertexShaderFile, const std::string& fragmentShaderFile);
