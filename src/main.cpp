#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/cimport.h>
#include <assimp/version.h>

#include <stdio.h>
#include <stdlib.h>

#include <vector>
#include <iostream>
using glm::mat4;
using glm::vec3;

//Struct to Pass in Values to shader through uniform buffer
struct PerFrameData
{
	mat4 MVP;			// model - view - projection matrix
	int isWireframe;	// set color of wireframe rendering 
};

//GLSL Code For the Vertex Shader
static const char* shaderCodeVertex = R"(
#version 460 core
layout(std140, binding = 0) uniform PerFrameData
{
	uniform mat4 MVP;
	uniform int isWireframe;
};
layout (location=0) in vec3 pos;
layout (location=0) out vec3 color;
void main()
{
	gl_Position = MVP * vec4(pos, 1.0);
	color = isWireframe > 0 ? vec3(0.0f) : pos.xyz;
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

int main(void)
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

	//Simple Empty
	//All the vertext Data is Inside the vertex shade
	GLuint vao;
	glCreateVertexArrays(1, &vao);
	glBindVertexArray(vao);

	//Buffer object to hold the data using DSA functions
	const GLsizeiptr kBufferSize = sizeof(PerFrameData);
	GLuint perFrameDataBuffer;
	glCreateBuffers(1, &perFrameDataBuffer);

	//GL_DYNAMIC_STORAGE_BIT tells OpenGL that the
	//content of the data store might be updated after
	//the creation through calls to glBufferSubData()
	glNamedBufferStorage(perFrameDataBuffer, kBufferSize, nullptr, GL_DYNAMIC_STORAGE_BIT);

	//Binds a range withing the buffer oject to an index buffer target
	//The buffer is bout to the indexed target 0, this will be used inside our shader
	glBindBufferRange(GL_UNIFORM_BUFFER, 0, perFrameDataBuffer, 0, kBufferSize);

	//Set Clear Color
	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);

	//Render a wireframe on top of the solid image without z-fighting
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_POLYGON_OFFSET_LINE);
	glPolygonOffset(-1.0f, -1.0f);

	GLuint meshData;
	glCreateBuffers(1, &meshData);

	//Ask library to convert geometric primitives into triangles
	const aiScene* scene = aiImportFile("../res/rubber_duck/scene.gltf", aiProcess_Triangulate);

	//Basic Error Checking
	if (!scene || !scene->HasMeshes())
	{
		printf("Unable to load res/rubber_duck/scene.gltf\n");
		exit(255);
	}

	//Convert 3D loaded scene into a data format to upload to OpenGL
	std::vector<vec3> positions;
	const aiMesh* mesh = scene->mMeshes[0];
	for (unsigned int i = 0; i != mesh->mNumFaces; i++)
	{
		const aiFace& face = mesh->mFaces[i];
		const unsigned int idx[3] = { face.mIndices[0], face.mIndices[1], face.mIndices[2] };
		//Flatten All indices and store only the vertex position
		for (int j = 0; j != 3; j++)
		{
			const aiVector3D v = mesh->mVertices[idx[j]];
			positions.push_back(vec3(v.x, v.z, v.y));
		}
	}
	//Deallocate The Scene Pointer
	aiReleaseImport(scene);

	//Upload content of positions[] to OpenGL Buffer
	glNamedBufferStorage(meshData, sizeof(vec3) * positions.size(), positions.data(), 0);

	glVertexArrayVertexBuffer(vao, 0, meshData, 0, sizeof(vec3));
	glEnableVertexArrayAttrib(vao, 0);
	glVertexArrayAttribFormat(vao, 0, 3, GL_FLOAT, GL_FALSE, 0);
	glVertexArrayAttribBinding(vao, 0, 0);

	//Save the number the of vertices
	const int numVertices = static_cast<int>(positions.size());

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
			glm::translate(mat4(1.0f), vec3(0.0f, -0.5f, -1.5f)),
			(float)glfwGetTime(), 
			vec3(0.0f, 1.0f, 0.0f));

		//Projection Matrix
		const mat4 p = glm::perspective(45.0f, ratio, 0.1f, 1000.0f);

		PerFrameData perFrameData = { 
			p * m,
			false };

		//Render Normalyy
		glUseProgram(program);
		glNamedBufferSubData(perFrameDataBuffer, 0, kBufferSize, &perFrameData);

		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		glDrawArrays(GL_TRIANGLES, 0, numVertices);

		//Render Wireframe
		perFrameData.isWireframe = true;
		glNamedBufferSubData(perFrameDataBuffer, 0, kBufferSize, &perFrameData);

		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		glDrawArrays(GL_TRIANGLES, 0, numVertices);

		//Swapping with the Back Buffer to view the render
		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	//Cleaning Up
	glDeleteBuffers(1, &meshData);
	glDeleteBuffers(1, &perFrameDataBuffer);
	glDeleteProgram(program);
	glDeleteShader(shaderFragment);
	glDeleteShader(shaderVertex);
	glDeleteVertexArrays(1, &vao);

	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}