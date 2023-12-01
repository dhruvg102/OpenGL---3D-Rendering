#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/cimport.h>
#include <assimp/version.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include "GLShader.cpp"

#include <stdio.h>
#include <stdlib.h>

#include <vector>

using glm::mat4;
using glm::vec3;
using glm::vec2;

//Struct to Pass in Values to shader through uniform buffer
struct PerFrameData
{
	mat4 MVP;			// model - view - projection matrix
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

	GLShader shaderVertex("../res/shaders/GL02.vert");
	GLShader shaderGeometry("../res/shaders/GL02.geom");
	GLShader shaderFragment("../res/shaders/GL02.frag");
	GLProgram program(shaderVertex, shaderGeometry, shaderFragment);
	program.useProgram();

	//Buffer object to hold the data using DSA functions
	const GLsizeiptr kUniformBufferSize = sizeof(PerFrameData);
	GLuint perFrameDataBuffer;
	glCreateBuffers(1, &perFrameDataBuffer);

	//GL_DYNAMIC_STORAGE_BIT tells OpenGL that the
	//content of the data store might be updated after
	//the creation through calls to glBufferSubData()
	glNamedBufferStorage(perFrameDataBuffer, kUniformBufferSize, nullptr, GL_DYNAMIC_STORAGE_BIT);

	//Binds a range withing the buffer oject to an index buffer target
	//The buffer is bout to the indexed target 0, this will be used inside our shader
	glBindBufferRange(GL_UNIFORM_BUFFER, 0, perFrameDataBuffer, 0, kUniformBufferSize);

	//Set Clear Color
	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);

	//Render a wireframe on top of the solid image without z-fighting
	glEnable(GL_DEPTH_TEST);

	//Ask library to convert geometric primitives into triangles
	const aiScene* scene = aiImportFile("../res/rubber_duck/scene.gltf", aiProcess_Triangulate);

	//Basic Error Checking
	if (!scene || !scene->HasMeshes())
	{
		printf("Unable to load res/rubber_duck/scene.gltf\n");
		exit(255);
	}

	struct VertexData
	{
		vec3 pos;
		vec2 tc;
	};

	//Convert 3D loaded scene into a data format to upload to OpenGL
	const aiMesh* mesh = scene->mMeshes[0];
	std::vector<VertexData> vertices;
	for (unsigned i = 0; i != mesh->mNumVertices; i++)
	{
		const aiVector3D v = mesh->mVertices[i];
		const aiVector3D t = mesh->mTextureCoords[0][i];
		vertices.push_back({ vec3(v.x, v.z, v.y), vec2(t.x, t.y) });
	}
	std::vector<unsigned int> indices;
	for (unsigned i = 0; i != mesh->mNumFaces; i++)
	{
		for (unsigned j = 0; j != 3; j++)
			indices.push_back(mesh->mFaces[i].mIndices[j]);
	}
	
	//Deallocate the scene pointer
	aiReleaseImport(scene);

	const size_t kSizeIndices = sizeof(unsigned int) * indices.size();
	const size_t kSizeVertices = sizeof(VertexData) * vertices.size();

	// indices
	GLuint dataIndices;
	glCreateBuffers(1, &dataIndices);

	//Upload content of indices to OpenGL Buffer
	glNamedBufferStorage(dataIndices, kSizeIndices, indices.data(), 0);

	GLuint vao;
	glCreateVertexArrays(1, &vao);
	glBindVertexArray(vao);
	glVertexArrayElementBuffer(vao, dataIndices);

	// vertices
	GLuint dataVertices;
	glCreateBuffers(1, &dataVertices);
	glNamedBufferStorage(dataVertices, kSizeVertices, vertices.data(), 0);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, dataVertices);

	// texture
	int w, h, comp;
	const uint8_t* img = stbi_load("../res/rubber_duck/textures/Duck_baseColor.png", &w, &h, &comp, 3);

	GLuint texture;
	glCreateTextures(GL_TEXTURE_2D, 1, &texture);
	glTextureParameteri(texture, GL_TEXTURE_MAX_LEVEL, 0);
	glTextureParameteri(texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTextureParameteri(texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTextureStorage2D(texture, 1, GL_RGB8, w, h);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTextureSubImage2D(texture, 0, 0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, img);
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
			glm::translate(mat4(1.0f), vec3(0.0f, -0.5f, -1.5f)),
			(float)glfwGetTime(), 
			vec3(0.0f, 1.0f, 0.0f));

		//Projection Matrix
		const mat4 p = glm::perspective(45.0f, ratio, 0.1f, 1000.0f);

		PerFrameData perFrameData = { p * m };

		//Render Normalyy
		glNamedBufferSubData(perFrameDataBuffer, 0, kUniformBufferSize, &perFrameData);
		glDrawElements(GL_TRIANGLES, static_cast<unsigned>(indices.size()), GL_UNSIGNED_INT, nullptr);
		//Swapping with the Back Buffer to view the render
		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	//Cleaning Up
	glDeleteBuffers(1, &dataIndices);
	glDeleteBuffers(1, &dataVertices);
	glDeleteBuffers(1, &perFrameDataBuffer);
	glDeleteVertexArrays(1, &vao);


	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}