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
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

#include "Utility/GLShader.cpp"
#include "Utility/UtilsMath.h"
#include "Bitmap.h"
#include "Utility/UtilsCubemap.cpp"

#include "Utility/debug.h"

#include <stdio.h>
#include <stdlib.h>

#include <vector>

using glm::mat4;
using glm::vec2;
using glm::vec3;
using glm::vec4;
using glm::ivec2;
//Struct to Pass in Values to shader through uniform buffer
struct PerFrameData
{
	mat4 model;
	mat4 MVP;			// model - view - projection matrix
	vec4 camera;
};

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

	initDebug();

	GLShader shdModelVertex("../res/shaders/GL03_duck.vert");
	GLShader shdModelFragment("../res/shaders/GL03_duck.frag");
	GLProgram progModel(shdModelVertex, shdModelFragment);

	GLShader shdCubeVertex("../res/shaders/GL03_cube.vert");
	GLShader shdCubeFragment("../res/shaders/GL03_cube.frag");
	GLProgram progCube(shdCubeVertex, shdCubeFragment);

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
		vec3 n;
		vec2 tc;
	};

	//Convert 3D loaded scene into a data format to upload to OpenGL
	const aiMesh* mesh = scene->mMeshes[0];
	std::vector<VertexData> vertices;
	for (unsigned i = 0; i != mesh->mNumVertices; i++)
	{
		const aiVector3D v = mesh->mVertices[i];
		const aiVector3D n = mesh->mNormals[i];
		const aiVector3D t = mesh->mTextureCoords[0][i];
		vertices.push_back({  vec3(v.x, v.z, v.y), vec3(n.x, n.y, n.z), vec2(t.x, t.y) });
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

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);


	// texture
	GLuint texture;
	{
		int w, h, comp;
		const uint8_t* img = stbi_load("../res/rubber_duck/textures/Duck_baseColor.png", &w, &h, &comp, 3);

		glCreateTextures(GL_TEXTURE_2D, 1, &texture);
		glTextureParameteri(texture, GL_TEXTURE_MAX_LEVEL, 0);
		glTextureParameteri(texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteri(texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTextureStorage2D(texture, 1, GL_RGB8, w, h);
		glTextureSubImage2D(texture, 0, 0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, img);
		glBindTextures(0, 1, &texture);

		stbi_image_free((void*)img);
	}

	// cube map
	GLuint cubemapTex;
	{
		int w, h, comp;
		const float* img = stbi_loadf("../res/piazza_bologni_1k.hdr", &w, &h, &comp, 3);
		Bitmap in(w, h, comp, eBitmapFormat_Float, img);
		Bitmap out = convertEquirectangularMapToVerticalCross(in);
		stbi_image_free((void*)img);

		stbi_write_hdr("screenshot.hdr", out.w_, out.h_, out.comp_, (const float*)out.data_.data());

		Bitmap cubemap = convertVerticalCrossToCubeMapFaces(out);

		glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &cubemapTex);
		glTextureParameteri(cubemapTex, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTextureParameteri(cubemapTex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTextureParameteri(cubemapTex, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		glTextureParameteri(cubemapTex, GL_TEXTURE_BASE_LEVEL, 0);
		glTextureParameteri(cubemapTex, GL_TEXTURE_MAX_LEVEL, 0);
		glTextureParameteri(cubemapTex, GL_TEXTURE_MAX_LEVEL, 0);
		glTextureParameteri(cubemapTex, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteri(cubemapTex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTextureStorage2D(cubemapTex, 1, GL_RGB32F, cubemap.w_, cubemap.h_);
		const uint8_t* data = cubemap.data_.data();

		for (unsigned i = 0; i != 6; ++i)
		{
			glTextureSubImage3D(cubemapTex, 0, 0, 0, i, cubemap.w_, cubemap.h_, 1, GL_RGB, GL_FLOAT, data);
			data += cubemap.w_ * cubemap.h_ * cubemap.comp_ * Bitmap::getBytesPerComponent(cubemap.fmt_);
		}
		glBindTextures(1, 1, &cubemapTex);
	}

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

		

		//Projection Matrix
		const mat4 p = glm::perspective(45.0f, ratio, 0.1f, 1000.0f);

		{
			//Model Matrix Calculated as a rotation around the diagonal (1,1,1)
			const mat4 m = glm::rotate(
				glm::translate(mat4(1.0f), vec3(0.0f, -0.5f, -1.5f)),
				(float)glfwGetTime(),
				vec3(0.0f, 1.0f, 0.0f));
			const PerFrameData perFrameData = {  m,  p * m, vec4(0.0f) };
			glNamedBufferSubData(perFrameDataBuffer, 0, kUniformBufferSize, &perFrameData);
			progModel.useProgram();
			glDrawElements(GL_TRIANGLES, static_cast<unsigned>(indices.size()), GL_UNSIGNED_INT, nullptr);
		}
		{
			const mat4 m = glm::scale(mat4(1.0f), vec3(2.0f));
			const PerFrameData perFrameData = { m,  p * m, vec4(0.0f) };
			glNamedBufferSubData(perFrameDataBuffer, 0, kUniformBufferSize, &perFrameData);
			progCube.useProgram();
			glDrawArrays(GL_TRIANGLES, 0, 36);
		}

		
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