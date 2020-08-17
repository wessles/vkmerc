#pragma once

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

class FlyCam
{
private:
	GLFWwindow *window;
	glm::vec4 position = glm::vec4(0.0, 0.0, 0.0, 1.0);
	glm::vec4 velocity = glm::vec4(0.0, 0.0, 0.0, 0.0);
	glm::vec3 rotation = glm::vec3(0.0, 0.0, 0.0);
	float FOV;
	bool wasPressingEscape = false;
	bool grabbed = false;
	double lastMouseX = 0, lastMouseY = 0;

	void setGrabbed(bool);
public:
	FlyCam();
	FlyCam(GLFWwindow*);
	void update();
	glm::mat4 getTransform();
	glm::mat4 getProjMatrix(float width, float height, float zMin = 0.01f, float zMax = 20.0f);
};

