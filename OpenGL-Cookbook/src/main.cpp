#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <filesystem>

#include <stb_image.h>

#include <stdio.h>
#include <stdlib.h>

#include <vector>
#include <iostream>

using glm::vec3;
using glm::mat4;

//Struct to Pass in Values to shader through uniform buffer
struct PerFrameData 
{
	mat4 MVP;			// model - view - projection matrix
	int isWireframe;	// set color of wireframe rendering 
};

//GLSL Code For the Vertex Shader
static const char* shaderCodeVertex = R"(
#version 460 core
layout(std140, binding = 0) uniform PerFrameData {
	uniform mat4 MVP;
	uniform int isWireframe;
};
layout (location=0) out vec3 color;

const vec3 pos[8] = vec3[8](
	vec3(-1.0,-1.0, 1.0),
	vec3( 1.0,-1.0, 1.0),
	vec3( 1.0, 1.0, 1.0),
	vec3(-1.0, 1.0, 1.0),

	vec3(-1.0,-1.0,-1.0),
	vec3( 1.0,-1.0,-1.0),
	vec3( 1.0, 1.0,-1.0),
	vec3(-1.0, 1.0,-1.0)
);

const vec3 col[8] = vec3[8](
	vec3( 1.0, 0.0, 0.0),
	vec3( 0.0, 1.0, 0.0),
	vec3( 0.0, 0.0, 1.0),
	vec3( 1.0, 1.0, 0.0),

	vec3( 1.0, 1.0, 0.0),
	vec3( 0.0, 0.0, 1.0),
	vec3( 0.0, 1.0, 0.0),
	vec3( 1.0, 0.0, 0.0)
);

const int indices[36] = int[36](
	// front
	0, 1, 2, 2, 3, 0,
	// right
	1, 5, 6, 6, 2, 1,
	// back
	7, 6, 5, 5, 4, 7,
	// left
	4, 0, 3, 3, 7, 4,
	// bottom
	4, 5, 1, 1, 0, 4,
	// top
	3, 2, 6, 6, 7, 3
);

void main()
{
	int idx = indices[gl_VertexID];
	gl_Position = MVP * vec4(pos[idx], 1.0);
	color = isWireframe > 0 ? vec3(0.0) : col[idx];
}
)";

//GLSL Code for the Fragment Shader
static const char* shaderCodeFragment = R"(
#version 460 core
layout (location=0) in vec3 color;
layout (location=0) out vec4 out_FragColor;
void main()
{
	out_FragColor = vec4(color, 1.0);
};
)";


void ShaderErrorCheck(GLuint shader, const char* type)
{
	GLint isCompiled = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
	if (isCompiled == GL_FALSE)
	{
		GLint maxLength = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);

		// The maxLength includes the NULL character
		std::vector<GLchar> errorLog(maxLength);
		glGetShaderInfoLog(shader, maxLength, &maxLength, &errorLog[0]);

		printf(type);
		for (unsigned int i = 0; i < maxLength; i++) {
			std::cout << errorLog[i];
		}
		// Provide the infolog in whatever manor you deem best.
		// Exit with failure.
		glDeleteShader(shader); // Don't leak the shader.
		return;
	}
}

int main()
{
	//GLFW Error Callback via a simple Lambda
	glfwSetErrorCallback(
		[](int error, const char* description)
		{
			fprintf(stderr, "Error: %s\n", description);
		}
	);

	//GLFW Initialization
	if (!glfwInit())
		exit(EXIT_FAILURE);

	//Telling GLFW the OpenGL Version (OpenGL 4.6)
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	//Window Creation With GLFW
	GLFWwindow* window = glfwCreateWindow(1024, 768, "Simple example", nullptr, nullptr);
	if (!window)
	{
		glfwTerminate();
		exit(EXIT_FAILURE);
	}
	
	//GLFW Key Callback via a simple Lambda
	glfwSetKeyCallback(
		window,
		[](GLFWwindow* window, int key, int scancode, int action, int mods)
		{
			if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
				glfwSetWindowShouldClose(window, GLFW_TRUE);
		}
	);

	//Prepareing the OpenGL context and
	//Using Glad to import all OpengGL entry points & extensions
	glfwMakeContextCurrent(window);
	gladLoadGL();
	glfwSwapInterval(1);
	
	//Compiling and Linking shader to a program
	const GLuint shaderVertex = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(shaderVertex, 1, &shaderCodeVertex, nullptr);
	glCompileShader(shaderVertex);

	//ShaderErrorCheck(shaderVertex, "Vertex");

	const GLuint shaderFragment = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(shaderFragment, 1, &shaderCodeFragment, nullptr);
	glCompileShader(shaderFragment);

	//ShaderErrorCheck(shaderFragment, "Fragment");

	const GLuint program = glCreateProgram();
	glAttachShader(program, shaderVertex);
	glAttachShader(program, shaderFragment);

	glLinkProgram(program);
	glUseProgram(program);

	//Simple Empty VAO
	//All the vertext Data is Inside the vertex shader
	GLuint vao;
	glCreateVertexArrays(1, &vao);
	glBindVertexArray(vao);

	//Buffer object to hold the data using DSA functions
	const GLsizeiptr kBufferSize = sizeof(PerFrameData);
	GLuint perFrameDataBuf;
	glCreateBuffers(1, &perFrameDataBuf);
	
	//GL_DYNAMIC_STORAGE_BIT tells OpenGL that the
	//content of the data store might be updated after
	//the creation through calls to glBufferSubData()
	glNamedBufferStorage(perFrameDataBuf, kBufferSize, nullptr, GL_DYNAMIC_STORAGE_BIT);
	
	//Binds a range withing the buffer oject to an index buffer target
	//The buffer is bout to the indexed target 0, this will be used inside our shader
	glBindBufferRange(GL_UNIFORM_BUFFER, 0, perFrameDataBuf, 0, kBufferSize);


	//Set Clear Color
	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
	
	glEnable(GL_DEPTH_TEST);

	//Render a wireframe on top of the solid image without z-fighting
	glEnable(GL_POLYGON_OFFSET_LINE);
	glPolygonOffset(-1.0f, -1.0f);

	//Load an inmage as a 4-channel RGBA image
	int w, h, comp;
	const uint8_t* img = stbi_load("../res/AL_Bistro/Cobble_02B_Diff.png", &w, &h, &comp, 4);
	std::cout << comp;
	GLuint texture;
	glCreateTextures(GL_TEXTURE_2D, 1, &texture);
	glTextureParameteri(texture, GL_TEXTURE_MAX_LEVEL, 0);
	glTextureParameteri(texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTextureParameteri(texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTextureStorage2D(texture, 1, GL_RGBA, w, h);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTextureSubImage2D(texture, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, img);
	glBindTextures(0, 1, &texture);

	stbi_image_free((void*)img);


	while (!glfwWindowShouldClose(window))
	{


		//Simple Way to setup a resizable window by
		//reading he the width and height from GLFW
		int width, height;
		glfwGetFramebufferSize(window, &width, &height);
		const float ratio = width / (float)height;

		//Clear Screen
		glViewport(0, 0, width, height);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		//Model Matrix Calculated as a rotation around the diagonal (1,1,1)
		const mat4 m = glm::rotate(
			glm::translate(mat4(1.0f), vec3(0.0f, 0.0f, -3.5f)),
			float(glfwGetTime()),
			vec3(1.0f, 1.0f, 1.0f)
		);
		//Projection Matrix
		const mat4 p = glm::perspective(
			45.0f, ratio, 0.1f, 1000.0f
		);
		

		PerFrameData perFrameData = {
			p * m,
			false
		};
		//Render Normally
		{
			glNamedBufferSubData(perFrameDataBuf, 0, kBufferSize, &perFrameData);

			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
			glDrawArrays(GL_TRIANGLES, 0, 36);
		}
		//Render Wireframe
		{
			perFrameData.isWireframe = true;
			glNamedBufferSubData(perFrameDataBuf, 0, kBufferSize, &perFrameData);

			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
			glDrawArrays(GL_TRIANGLES, 0, 36);
		}
		//Swapping the Buffer  to view the render
		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	//Cleaning Up
	glDeleteBuffers(1, &perFrameDataBuf);
	glDeleteProgram(program);
	glDeleteShader(shaderFragment);
	glDeleteShader(shaderVertex);
	glDeleteVertexArrays(1, &vao);

	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}