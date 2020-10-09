#pragma once

#include <glfw3/glfw3.h>
#include <glm/glm.hpp>

class OrbitCam
{
private:
	GLFWwindow* window;
	float distance = 2;
	glm::vec3 rotation = glm::vec3(0.0, 0.0, 0.0);
	float FOV = 90;
	bool wasPressingEscape = false;
	bool grabbed = false;
	double lastMouseX = 0, lastMouseY = 0;

	void setGrabbed(bool);
public:
	OrbitCam(GLFWwindow*);
	void update();
	glm::mat4 getTransform();
	glm::mat4 getProjMatrix(float width, float height, float zMin = 0.01f, float zMax = 20.0f);
};

