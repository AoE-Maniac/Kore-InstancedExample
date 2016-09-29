#include "pch.h"

#include <Kore/System.h>
#include <Kore/Graphics/Graphics.h>
#include <Kore/Graphics/Shader.h>
#include <Kore/IO/FileReader.h>
#include <Kore/Math/Random.h>

using namespace Kore;

namespace {
	// Generate 100x100 instances
	static int INSTANCES_X = 100;
	static int INSTANCES_Z = 100;
	static int INSTANCES = INSTANCES_X * INSTANCES_Z;

	// Each cylinder has 32 sides generated, change to control level of detail / polygon count
	static int CYLINDER_SECTIONS = 32; // Each section results in 4 polygons

	vec4 cameraStart;
	mat4 view;
	mat4 projection;
	mat4 mvp;

	float* instancePositions;
	float* instanceYOffsets;

	Shader* vertexShader;
	Shader* fragmentShader;
	Program* program;

	VertexStructure** structures;
	VertexBuffer** vertexBuffers;
	IndexBuffer* indexBuffer;
	
	void update() {
		// Move camera and update view matrix
		vec4 newCameraPos = mat4::RotationY(System::time() / 4) * cameraStart;
		view = mat4::lookAt(newCameraPos, // Position in World Space
			vec3(0, 0, 0), // Looks at the origin
			vec3(0, 1, 0) // Up-vector
		);

		mat4 vp = projection * view;

		// Fill transformation matrix buffer with values from each instance
		float* data = vertexBuffers[1]->lock();
		for (int i = 0; i < INSTANCES; ++i) {
			// Update height value
			int offsetPosition = i * 3;
			instanceYOffsets[i] = Kore::sin(instancePositions[offsetPosition] * 4 + instancePositions[offsetPosition + 2] + System::time() * 2) / 4;

			// Calculate new model matrix
			mat4 m = mat4::Translation(
				instancePositions[offsetPosition    ],
				instancePositions[offsetPosition + 1] + instanceYOffsets[i],
				instancePositions[offsetPosition + 2]);
			mat4 mvp = vp * m;
			
			// Write to buffer
			int offsetBuffer = i * 19;
			data[offsetBuffer     ] = mvp[0][0];
			data[offsetBuffer +  1] = mvp[1][0];
			data[offsetBuffer +  2] = mvp[2][0];
			data[offsetBuffer +  3] = mvp[3][0];

			data[offsetBuffer +  4] = mvp[0][1];
			data[offsetBuffer +  5] = mvp[1][1];
			data[offsetBuffer +  6] = mvp[2][1];
			data[offsetBuffer +  7] = mvp[3][1];

			data[offsetBuffer +  8] = mvp[0][2];
			data[offsetBuffer +  9] = mvp[1][2];
			data[offsetBuffer + 10] = mvp[2][2];
			data[offsetBuffer + 11] = mvp[3][2];

			data[offsetBuffer + 12] = mvp[0][3];
			data[offsetBuffer + 13] = mvp[1][3];
			data[offsetBuffer + 14] = mvp[2][3];
			data[offsetBuffer + 15] = mvp[3][3];
		}
		vertexBuffers[1]->unlock();

		Graphics::begin();
		Graphics::clear(Graphics::ClearColorFlag | Graphics::ClearDepthFlag, 0xFFFFBD00, 1.0f);

		program->set();
		Graphics::setVertexBuffers(vertexBuffers, 2);
		Graphics::setIndexBuffer(*indexBuffer);
		Graphics::drawIndexedVerticesInstanced(INSTANCES);

		Graphics::end();
		Graphics::swapBuffers();
	}
}

void generateCylinerSection(vec3 lastPoint, vec3 nextPoint, float height, int index, float* vertices, int* indices) {
	int vertOffset = 3 * index;
	vertices[vertOffset     ] = lastPoint.x();
	vertices[vertOffset +  1] = 0;
	vertices[vertOffset +  2] = lastPoint.z();

	vertices[vertOffset +  3] = lastPoint.x();
	vertices[vertOffset +  4] = height;
	vertices[vertOffset +  5] = lastPoint.z();

	vertices[vertOffset +  6] = nextPoint.x();
	vertices[vertOffset +  7] = 0;
	vertices[vertOffset +  8] = nextPoint.z();

	vertices[vertOffset +  9] = nextPoint.x();
	vertices[vertOffset + 10] = height;
	vertices[vertOffset + 11] = nextPoint.z();

	int indOffset = 3 * (index - 2);
	// First part of side
	indices[indOffset     ] = index;
	indices[indOffset +  1] = index + 1;
	indices[indOffset +  2] = index + 2;

	// Second part of side
	indices[indOffset +  3] = index + 3;
	indices[indOffset +  4] = index + 2;
	indices[indOffset +  5] = index + 1;

	// Bottom
	indices[indOffset +  6] = 0;
	indices[indOffset +  7] = index;
	indices[indOffset +  8] = index + 2;

	// Top
	indices[indOffset +  9] = index + 3;
	indices[indOffset + 10] = index + 1;
	indices[indOffset + 11] = 1;
}

void generateCylinderMesh(float height, float radius, int sections, VertexStructure* structure, VertexBuffer** vertexBuffer, IndexBuffer** indexBuffer) {
	*vertexBuffer = new VertexBuffer(3 * (4 * sections + 2), *structure, 0);
	*indexBuffer = new IndexBuffer(3 * (4 * (sections + 1)));

	float* vertices = (*vertexBuffer)->lock();
	int* indices = (*indexBuffer)->lock();

	// Bottom center
	vertices[0] = 0;
	vertices[1] = 0;
	vertices[2] = 0;

	// Top center
	vertices[3] = 0;
	vertices[4] = height;
	vertices[5] = 0;

	int index = 2;
	vec3 firstPoint = vec3(0, 0, radius);
	vec3 lastPoint = firstPoint;
	vec3 nextPoint;
	for (int i = 0; i < sections; ++i) {
		nextPoint = mat3::RotationY(i * (2.0f / sections) * pi) * firstPoint;

		generateCylinerSection(lastPoint, nextPoint, height, index, vertices, indices);

		lastPoint = nextPoint;
		index += 4;
	}

	// Last part, close exactly (i.e. use first point)
	generateCylinerSection(lastPoint, firstPoint, height, index, vertices, indices);

	(*vertexBuffer)->unlock();
	(*indexBuffer)->unlock();
}


int kore(int argc, char** argv) {
	char* name = "Instanced Rendering Example";
	int width = 1024;
	int height = 768;

	Kore::System::setName(name);
	Kore::System::setup();
	Kore::WindowOptions options;
	options.title = name;
	options.width = width;
	options.height = height;
	options.x = 100;
	options.y = 100;
	options.targetDisplay = -1;
	options.mode = WindowModeWindow;
	options.rendererOptions.depthBufferBits = 16;
	options.rendererOptions.stencilBufferBits = 8;
	options.rendererOptions.textureFormat = 0;
	options.rendererOptions.antialiasing = 0;
	Kore::System::initWindow(options);

	FileReader vs("shader.vert");
	vertexShader = new Shader(vs.readAll(), vs.size(), VertexShader);
	FileReader fs("shader.frag");
	fragmentShader = new Shader(fs.readAll(), fs.size(), FragmentShader);

	vertexBuffers = new VertexBuffer*[2];

	// Mesh, is shared by all instances
	structures = new VertexStructure*[2];
	structures[0] = new VertexStructure();
	structures[0]->add("pos", Float3VertexData);

	generateCylinderMesh(1, 0.5f, CYLINDER_SECTIONS, structures[0], &vertexBuffers[0], &indexBuffer);

	// Position and color, is different for each instance
	structures[1] = new VertexStructure();
	structures[1]->add("m", Float4x4VertexData);
	structures[1]->add("col", Float3VertexData);

	vertexBuffers[1] = new VertexBuffer(INSTANCES, *structures[1], 1); // Changed after every instance, use int > 1 for a higher number for repetitions

	// Initialize colors
	Random::init(System::time() * 1000);
	float* cylinderData = vertexBuffers[1]->lock();
	for (int i = 0; i < INSTANCES; ++i) {
		cylinderData[i * 19 + 16] = 1;
		cylinderData[i * 19 + 17] = 0.75f + (float) Kore::Random::get(-100, 100) / 500;
		cylinderData[i * 19 + 18] = 0;
	}
	vertexBuffers[1]->unlock();
	// Position is not set during initialization, but updated for each frame
	
	program = new Program;
	program->setVertexShader(vertexShader);
	program->setFragmentShader(fragmentShader);
	program->link(structures, 2);

	Graphics::setRenderState(DepthTest, true);
	Graphics::setRenderState(DepthTestCompare, ZCompareLess);
	
	// Initialize data, not relevant for rendering
	instancePositions = new float[INSTANCES * 3];
	instanceYOffsets = new float[INSTANCES];
	for (int x = 0; x < INSTANCES_X; ++x) {
		for (int z = 0; z < INSTANCES_Z; ++z) {
			// Span x/z grid, center on 0/0
			int offset = 3 * (x * INSTANCES_X + z);
			instancePositions[offset    ] = x - ((float) INSTANCES_X - 1) / 2;
			instancePositions[offset + 1] = 0;
			instancePositions[offset + 2] = z - ((float) INSTANCES_Z - 1) / 2;
		}
	}

	cameraStart = vec4(0, 7.5f, 5);
	projection = mat4::Perspective(0.5f * pi, (float) width / height, 0.1f, 100.0f);

	Kore::System::setCallback(update);

	Kore::System::start();

	return 0;
}
