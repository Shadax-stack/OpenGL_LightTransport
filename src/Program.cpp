#include "Window.h"
#include "Renderer.h"
#include "Buffer.h"
#include "VertexArray.h"
#include "Shader.h"
#include "Texture.h"
#include "Camera.h"
#include "TimeUtil.h"
#include "Mesh.h"
#include "SceneManager.h"

float QuadVertices[] = {
	-1.0f,  1.0f,
	-1.0f, -1.0f,
	 1.0f, -1.0f,

	-1.0f,  1.0f,
	 1.0f, -1.0f,
	 1.0f,  1.0f
};

uint32_t Width = 1280;
uint32_t Height = 720;

// Camera params 

constexpr float CameraSpeed       = 2000.000f;
constexpr float CameraSensitivity = 0.001f;
glm::vec2 LastCursorPosition;

// I need class here because Intellisense is not detecting the camera type
class Camera Camera;

void MouseCallback(GLFWwindow* Window, double X, double Y) {
	glm::vec2 CurrentCursorPosition = glm::vec2(X, Y);

	glm::vec2 DeltaPosition = CurrentCursorPosition - LastCursorPosition;

	DeltaPosition.y = -DeltaPosition.y;
	//DeltaPosition.x = -DeltaPosition.x;

	DeltaPosition *= CameraSensitivity;

	Camera.AddRotation(glm::vec3(DeltaPosition, 0.0f));

	LastCursorPosition = CurrentCursorPosition;
}

struct RayBuffer {
	RayBuffer(void) {
		Origin.CreateBinding();
		Origin.LoadData   (GL_RGBA16F, GL_RGB, GL_HALF_FLOAT, Width, Height, nullptr);

		Direction.CreateBinding();
		Direction.LoadData(GL_RGBA16F, GL_RGB, GL_HALF_FLOAT, Width, Height, nullptr);

	}
	Texture2D Origin;
	Texture2D Direction;
};

int main() {
	Window Window;
	Window.Open("OpenGL Light Transport", Width, Height);

	Renderer Renderer;
	Renderer.Init(&Window);

	Buffer FullscreenQuadData;
	FullscreenQuadData.CreateBinding(BUFFER_TARGET_ARRAY);
	FullscreenQuadData.UploadData(sizeof(QuadVertices), QuadVertices);

	VertexArray FullscreenQuad;
	FullscreenQuad.CreateBinding();

	FullscreenQuad.CreateStream(0, 2, 2 * sizeof(float));

	Texture2D RenderTargetColor;
	RenderTargetColor.CreateBinding();
	RenderTargetColor.LoadData(GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, Width, Height, nullptr);

	Texture2D IntersectionDepth;
	IntersectionDepth.CreateBinding();
	IntersectionDepth.LoadData(GL_R32F, GL_RED, GL_FLOAT, Width, Height, nullptr);

	ShaderRasterization PresentShader;
	PresentShader.CompileFiles("res/shaders/present/Present.vert", "res/shaders/present/Present.frag");

	ShaderCompute FiniteAperture;
	FiniteAperture.CompileFile("res/shaders/kernel/raygen/FiniteAperture.comp");

	ShaderCompute ClosestHit;
	ClosestHit.CompileFile("res/shaders/kernel/intersect/Closest.comp");

	RayBuffer PrimaryRayBuffer;

	Camera.GenerateViewTransform();
	Camera.UpdateImagePlaneParameters((float)Width / (float)Height, glm::radians(45.0f));
	LastCursorPosition = glm::vec2(Width, Height) / 2.0f;
	Window.SetInputCallback(MouseCallback);

	Camera.SetPosition(glm::vec3(0.0f, 0.15f, 0.5f) * 6.0f);

	SceneManager Scene;

	Scene.LoadScene("res/objects/crytek/Sponza.obj");

	Timer FrameTimer;

	while (!Window.ShouldClose()) {
		FrameTimer.Begin();

		Renderer.Begin();

		if (Window.GetKey(GLFW_KEY_W)) {
			Camera.Move( CameraSpeed * (float)FrameTimer.Delta);
		} else if (Window.GetKey(GLFW_KEY_S))  {
			Camera.Move(-CameraSpeed * (float)FrameTimer.Delta);
		}
		Camera.GenerateViewTransform();
		Camera.GenerateImagePlane();

		const float Clear = 1e20f;
		glClearTexImage(IntersectionDepth.GetHandle(), 0, GL_RED, GL_FLOAT, &Clear);

		FiniteAperture.CreateBinding();
		FiniteAperture.LoadImage2D("RayOrigin"   , PrimaryRayBuffer.Origin   );
		FiniteAperture.LoadImage2D("RayDirection", PrimaryRayBuffer.Direction);
		FiniteAperture.LoadCamera ("Camera", Camera);

		glDispatchCompute(Width / 8, Height / 8, 1);
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

		for (auto MeshIter = Scene.StartMeshIterator(); MeshIter != Scene.StopMeshIterator(); MeshIter++) {
			ClosestHit.CreateBinding();
			ClosestHit.LoadImage2D("RayOrigin", PrimaryRayBuffer.Origin);
			ClosestHit.LoadImage2D("RayDirection", PrimaryRayBuffer.Direction);
			ClosestHit.LoadImage2D("ColorOutput", RenderTargetColor);
			ClosestHit.LoadImage2D("IntersectionDepth", IntersectionDepth, GL_R32F);
			ClosestHit.LoadMesh("Mesh", "BVH", "Material", *MeshIter);

			glDispatchCompute(Width / 8, Height / 8, 1);
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
		}

		PresentShader.CreateBinding();
		PresentShader.LoadTexture2D("ColorTexture", RenderTargetColor);

		glDrawArrays(GL_TRIANGLES, 0, 6);

		Renderer.End();
		Window.Update();

		FrameTimer.End();

		FrameTimer.DebugTime();
	}

	FiniteAperture.FreeBinding();
	FiniteAperture.Free();

	ClosestHit.FreeBinding();
	ClosestHit.Free();

	PresentShader.FreeBinding();
	PresentShader.Free();
	
	RenderTargetColor.FreeBinding();
	RenderTargetColor.Free();

	FullscreenQuad.FreeBinding();
	FullscreenQuadData.FreeBinding();

	FullscreenQuad.Free();
	FullscreenQuadData.Free();

	Renderer.Free();
	Window.Close();

}