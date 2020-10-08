#ifndef _GLUTILS_HPP__
#define _GLUTILS_HPP__

#include <cstdlib>
#include <sstream>
#include <vector>
#include <assert.h>
#include <iostream>
#include <fstream>

#include <GL/glew.h>

#include "GLUtils/Program.hpp"
#include "GLUtils/VBO.hpp"
#include "GameException.h"

#define BUFFER_OFFSET(i) ((char *)NULL + (i))
#define CHECK_GL_ERRORS() GLUtils::checkGLErrors(__FILE__, __LINE__)
#define CHECK_GL_FBO_COMPLETENESS() GLUtils::checkGLFBOCompleteness(__FILE__, __LINE__)

namespace GLUtils
{
	inline void getActiveUniforms(const GLuint prog){
		GLint numUniforms = 0;
		glGetProgramInterfaceiv(prog, GL_UNIFORM, GL_ACTIVE_RESOURCES, &numUniforms);
		const GLenum properties[4] = { GL_BLOCK_INDEX , GL_TYPE , GL_NAME_LENGTH , GL_LOCATION };
		std::cout << "Active uniforms: \n";
		for(int unif = 0; unif < numUniforms; ++unif){
			GLint values[4];
			glGetProgramResourceiv(prog, GL_UNIFORM, unif, 4, properties, 4, nullptr, values);

			//Skip any uniforms that are in a block.
			if(values[0] != -1)
				continue;

			std::vector<char> nameData(values[2]);
			glGetProgramResourceName(prog, GL_UNIFORM, unif, nameData.size(), nullptr, &nameData[0]);
			std::string name(nameData.begin(), nameData.end() - 1);
			std::cout << name << '\n';
		}
	}

	inline void checkGLErrors(const char *file, unsigned int line){
		GLenum ASSERT_GL_err = glGetError();
		if(ASSERT_GL_err != GL_NO_ERROR){
			std::stringstream ASSERT_GL_string;
			ASSERT_GL_string << file << '@' << line << ": OpenGL error:"
					<< std::hex << ASSERT_GL_err << " ";// << gluErrorString(ASSERT_GL_err);
			THROW_EXCEPTION( ASSERT_GL_string.str() );
		}
	}

	inline void checkGLFBOCompleteness(const char *file, unsigned int line){
		GLenum err = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if(err != GL_FRAMEBUFFER_COMPLETE){
			std::stringstream log;
			log << file << '@' << line << ": FBO incomplete error:"
					<< std::hex << err << " " << gluErrorString(err);
			throw std::runtime_error(log.str());
		}
	}

#define CHECK_GL_ERROR() GLUtils::checkGLErrors(__FILE__, __LINE__)

	inline std::string readFile(std::string file){
		int length;
		std::string buffer;
		std::string contents;

		std::ifstream is;
		is.open(file.c_str());

		if(!is.good()){
			std::string err = "Could not open ";
			err.append(file);
			THROW_EXCEPTION(err);
		}

		// get length of file:
		is.seekg(0, std::ios::end);
		length = static_cast<int>(is.tellg());
		is.seekg(0, std::ios::beg);

		// reserve memory:
		contents.reserve(length);

		// read data
		while(getline(is, buffer)){
			contents.append(buffer);
			contents.append("\n");
		}
		is.close();

		return contents;
	}
}; //Namespace GLUtils
#endif
