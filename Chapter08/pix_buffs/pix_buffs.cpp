#include "pix_buffs.h"
#include <GL\glu.h>
#include <stdio.h>
#include <iostream>

static GLfloat vGreen[] = { 0.0f, 1.0f, 0.0f, 1.0f };
static GLfloat vWhite[] = { 1.0f, 1.0f, 1.0f, 1.0f };
static GLfloat vLightPos[] = { 0.0f, 3.0f, 0.0f, 1.0f };


////////////////////////////////////////////////////////////////////////////
// Do not put any OpenGL code here. General guidence on constructors in 
// general is to not put anything that can fail here either (opening files,
// allocating memory, etc.)
PixBuffs::PixBuffs(void) : screenWidth(800), screenHeight(600), bFullScreen(false), 
				bAnimated(true), bUsePBOPath(false), blurProg(0), speedFactor(1.0f)
{

}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Load in a BMP file as a texture. Allows specification of the filters and the wrap mode
bool PixBuffs::LoadBMPTexture(const char *szFileName, GLenum minFilter, GLenum magFilter, GLenum wrapMode)	
{
	BYTE *pBits;
	GLint iWidth, iHeight;

	pBits = gltReadBMPBits(szFileName, &iWidth, &iHeight);
	if(pBits == NULL)
		return false;

	// Set Wrap modes
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapMode);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapMode);

	// Do I need to generate mipmaps?
	if(minFilter == GL_LINEAR_MIPMAP_LINEAR || minFilter == GL_LINEAR_MIPMAP_NEAREST || minFilter == GL_NEAREST_MIPMAP_LINEAR || minFilter == GL_NEAREST_MIPMAP_NEAREST)
		glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, iWidth, iHeight, 0, GL_BGR, GL_UNSIGNED_BYTE, pBits);
	return true;
}


///////////////////////////////////////////////////////////////////////////////
// OpenGL related startup code is safe to put here. Load textures, etc.
void PixBuffs::Initialize(void)
{
    GLenum err = glewInit();
	if (GLEW_OK != err)
	{
		/* Problem: glewInit failed, something is seriously wrong. */
		fprintf(stderr, "Error: %s\n", glewGetErrorString(err));
	}

	// Initialze Shader Manager
	shaderManager.InitializeStockShaders();
	glEnable(GL_DEPTH_TEST);

	// Black
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

	gltMakeTorus(torusBatch, 0.4f, 0.15f, 35, 35);

	GLfloat alpha = 0.25f;
	floorBatch.Begin(GL_TRIANGLE_FAN, 4, 1);
		floorBatch.Color4f(0.0f, 1.0f, 0.0f, alpha);
		floorBatch.MultiTexCoord2f(0, 0.0f, 0.0f);
		floorBatch.Normal3f(0.0, 1.0f, 0.0f);
		floorBatch.Vertex3f(-20.0f, -0.41f, 20.0f);

		floorBatch.Color4f(0.0f, 1.0f, 0.0f, alpha);
		floorBatch.MultiTexCoord2f(0, 10.0f, 0.0f);
		floorBatch.Normal3f(0.0, 1.0f, 0.0f);
		floorBatch.Vertex3f(20.0f, -0.41f, 20.0f);

		floorBatch.Color4f(0.0f, 1.0f, 0.0f, alpha);
		floorBatch.MultiTexCoord2f(0, 10.0f, 10.0f);
		floorBatch.Normal3f(0.0, 1.0f, 0.0f);
		floorBatch.Vertex3f(20.0f, -0.41f, -20.0f);

		floorBatch.Color4f(0.0f, 1.0f, 0.0f, alpha);
		floorBatch.MultiTexCoord2f(0, 0.0f, 10.0f);
		floorBatch.Normal3f(0.0, 1.0f, 0.0f);
		floorBatch.Vertex3f(-20.0f, -0.41f, -20.0f);
	floorBatch.End();
	
	glGenTextures(1, textures);
	glBindTexture(GL_TEXTURE_2D, textures[0]);
	LoadBMPTexture("marble.bmp", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR, GL_REPEAT);

	// Create blur program
	blurProg =  gltLoadShaderPairWithAttributes("blur.vs", "blur.fs", 2,
												GLT_ATTRIBUTE_VERTEX, "vVertex", GLT_ATTRIBUTE_TEXTURE0, "texCoord0");

	// Create blur textures
	glGenTextures(6, blurTextures);

    // XXX I don't think this is necessary. Should set texture data to NULL
	// Allocate a pixel buffer to initialize textures and PBOs
	pixelDataSize = GetWidth()*GetHeight()*3*sizeof(unsigned int); // XXX This should be unsigned byte
	void* data = (void*)malloc(pixelDataSize);
	memset(data, 0x00, pixelDataSize);

	// Setup 6 texture units for blur effect
	// Initialize texture data
	for (int i=0; i<6;i++)
	{
		glActiveTexture(GL_TEXTURE1+i);
		glBindTexture(GL_TEXTURE_2D, blurTextures[i]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, GetWidth(), GetHeight(), 0, GL_RGB, GL_UNSIGNED_BYTE, data);
	}

	// Alloc space for copying pixels so we dont call malloc on every draw
	glGenBuffers(1, pixBuffObjs);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, pixBuffObjs[0]);
	glBufferData(GL_PIXEL_PACK_BUFFER, pixelDataSize, pixelData, GL_DYNAMIC_COPY);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

	// Create geometry and a matrix for screen aligned drawing
	gltGenerateOrtho2DMat(GetWidth(), GetHeight(), orthoMatrix, screenQuad);

	// Make sure all went well
	gltCheckErrors();
}


///////////////////////////////////////////////////////////////////////////////
// Do your cleanup here. Free textures, display lists, buffer objects, etc.
void PixBuffs::Shutdown(void)
{
	// Make sure default FBO is bound
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

	// Cleanup textures
	for (int i=0; i<7;i++)
	{
		glActiveTexture(GL_TEXTURE0+i);
		glBindTexture(GL_TEXTURE_2D, 0);
	}
	
	// Now delete detached textures
	glDeleteTextures(1, textures);
	glDeleteTextures(6, blurTextures);

	// delete PBO
	glDeleteBuffers(1, pixBuffObjs);
}


///////////////////////////////////////////////////////////////////////////////
// This is called at least once and before any rendering occurs. If the screen
// is a resizeable window, then this will also get called whenever the window
// is resized.
void PixBuffs::Resize(GLsizei nWidth, GLsizei nHeight)
{
	glViewport(0, 0, nWidth, nHeight);
	transformPipeline.SetMatrixStacks(modelViewMatrix, projectionMatrix);
 	viewFrustum.SetPerspective(35.0f, float(nWidth)/float(nHeight), 1.0f, 100.0f);
	projectionMatrix.LoadMatrix(viewFrustum.GetProjectionMatrix());
	modelViewMatrix.LoadIdentity();

	// update screen sizes
	screenWidth = nWidth;
	screenHeight = nHeight;

	// reset screen aligned quad
	gltGenerateOrtho2DMat(GetWidth(), GetHeight(), orthoMatrix, screenQuad);

	free(pixelData);
	pixelDataSize = GetWidth()*GetHeight()*3*sizeof(unsigned int);
	pixelData = (void*)malloc(pixelDataSize);

	//  Resize PBOs
	glBindBuffer(GL_PIXEL_PACK_BUFFER, pixBuffObjs[0]);
	glBufferData(GL_PIXEL_PACK_BUFFER, pixelDataSize, pixelData, GL_DYNAMIC_COPY);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

	gltCheckErrors();
}


///////////////////////////////////////////////////////////////////////////////
// Update the camera based on user input, toggle display modes
// 
void PixBuffs::MoveCamera(void)
{ 
	static CStopWatch cameraTimer;
	float fTime = cameraTimer.GetElapsedSeconds();
	float linear = fTime * 12.0f;
	cameraTimer.Reset(); 

	// Alternate between PBOs and local memory when 'P' is pressed
	static bool bLastKeyP = false;
	if(GetAsyncKeyState('P') || GetAsyncKeyState('p'))
	{
		if(bLastKeyP == false)
			bUsePBOPath = (bUsePBOPath)? GL_FALSE : GL_TRUE;

		bLastKeyP = true;
	}
	else
	{
		bLastKeyP = false;
	}

	// Speed up movement
	if(GetAsyncKeyState(VK_ADD))
	{
		speedFactor += linear/2;
		if(speedFactor > 6)
			speedFactor = 6;
	}

	// Slow down moement
	if(GetAsyncKeyState(VK_SUBTRACT))
	{
		speedFactor -= linear/2;
		if(speedFactor < 0.5)
			speedFactor = 0.5;
	}
}

///////////////////////////////////////////////////////////////////////////////
// Load and setup program for blur effect
// 

void PixBuffs::SetupBlurProg(void)
{
	// Set the blur program as the current one
	glUseProgram(blurProg);

	// Set MVP matrix
	glUniformMatrix4fv(glGetUniformLocation(blurProg, "mvpMatrix"), 1, GL_FALSE, transformPipeline.GetModelViewProjectionMatrix());

	// Setup the textue units for the blur targets, these rotate every frame
	glUniform1i(glGetUniformLocation(blurProg, "textureUnit0"), GetBlurTarget0());
	glUniform1i(glGetUniformLocation(blurProg, "textureUnit1"), GetBlurTarget1());
	glUniform1i(glGetUniformLocation(blurProg, "textureUnit2"), GetBlurTarget2());
	glUniform1i(glGetUniformLocation(blurProg, "textureUnit3"), GetBlurTarget3());
	glUniform1i(glGetUniformLocation(blurProg, "textureUnit4"), GetBlurTarget4());
	glUniform1i(glGetUniformLocation(blurProg, "textureUnit5"), GetBlurTarget5());
}


///////////////////////////////////////////////////////////////////////////////
// Draw the scene 
// 
void PixBuffs::DrawWorld(GLfloat yRot, GLfloat xPos)
{
	M3DMatrix44f mCamera;
	modelViewMatrix.GetMatrix(mCamera);
	
	// Need light position relative to the Camera
	M3DVector4f vLightTransformed;
	m3dTransformVector4(vLightTransformed, vLightPos, mCamera);

	// Draw stuff relative to the camera
	modelViewMatrix.PushMatrix();
		modelViewMatrix.Translate(0.0f, 0.2f, -2.5f);
		modelViewMatrix.Translate(xPos, 0.0f, 0.0f);
		modelViewMatrix.Rotate(yRot, 0.0f, 1.0f, 0.0f);
	
		shaderManager.UseStockShader(GLT_SHADER_POINT_LIGHT_DIFF, 
				modelViewMatrix.GetMatrix(), 
				transformPipeline.GetProjectionMatrix(), 
				vLightTransformed, vGreen, 0);
		torusBatch.Draw();
	modelViewMatrix.PopMatrix();
}


///////////////////////////////////////////////////////////////////////////////
// Render a frame. The owning framework is responsible for buffer swaps,
// flushes, etc.
void PixBuffs::Render(void)
{
	static CStopWatch animationTimer;
	static float totalTime = 6; // To go back and forth
	static float halfTotalTime = totalTime/2;
	float seconds = animationTimer.GetElapsedSeconds() * speedFactor;
	float xPos = 0;

	// Calculate the next postion of the moving object
	// First perform a mod-like operation on the time as a float
	while(seconds > totalTime)
		seconds -= totalTime;

	// Move object position, if it's gone half way across
	// start bringing it back
	if(seconds < halfTotalTime)
		xPos = seconds -halfTotalTime*0.5f;
	else
		xPos = totalTime - seconds -halfTotalTime*0.5f;

	MoveCamera();

	// First draw world to screen
	modelViewMatrix.PushMatrix();	
		M3DMatrix44f mCamera;
		cameraFrame.GetCameraMatrix(mCamera);
		modelViewMatrix.MultMatrix(mCamera);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, textures[0]); // Marble
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		shaderManager.UseStockShader(GLT_SHADER_TEXTURE_MODULATE, transformPipeline.GetModelViewProjectionMatrix(), vWhite, 0);

		floorBatch.Draw();
		DrawWorld(0.0f, xPos);
	modelViewMatrix.PopMatrix();
	
	if(bUsePBOPath)
	{
		// First bind the PBO as the pack buffer, then read the pixels directly to the PBO
		glBindBuffer(GL_PIXEL_PACK_BUFFER, pixBuffObjs[0]);
		glReadPixels(0, 0, GetWidth(), GetHeight(), GL_RGB, GL_UNSIGNED_BYTE, NULL);
		glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

		// Next bind the PBO as the unpack buffer, then push the pixels straight into the texture
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pixBuffObjs[0]);
        
        // Setup texture unit for new blur, this gets imcremented every frame 
		glActiveTexture(GL_TEXTURE0+GetBlurTarget0() ); 
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, GetWidth(), GetHeight(), 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
	}
	else
	{
		// Grab the screen pixels and copy into local memory
		glReadPixels(0, 0, GetWidth(), GetHeight(), GL_RGB, GL_UNSIGNED_BYTE, pixelData);
		
		// Push pixels from client memory into texture
        // Setup texture unit for new blur, this gets imcremented every frame
		glActiveTexture(GL_TEXTURE0+GetBlurTarget0() );
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, GetWidth(), GetHeight(), 0, GL_RGB, GL_UNSIGNED_BYTE, pixelData);
	}

	// Draw full screen quad with blur shader and all blur textures
	projectionMatrix.PushMatrix(); 
		projectionMatrix.LoadIdentity();
		projectionMatrix.LoadMatrix(orthoMatrix);
		modelViewMatrix.PushMatrix();	
			modelViewMatrix.LoadIdentity();
			glDisable(GL_DEPTH_TEST); 
			SetupBlurProg();
			screenQuad.Draw();
			glEnable(GL_DEPTH_TEST); 
		modelViewMatrix.PopMatrix(); 
	projectionMatrix.PopMatrix();

	// Move to the next blur texture for the next frame
	AdvanceBlurTaget();
}
