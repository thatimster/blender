#ifndef __KX_RENDER_DATA_H__
#define __KX_RENDER_DATA_H__

#include "RAS_Rasterizer.h"
#include "RAS_FramingManager.h"
#include "SG_Frustum.h"

#include <functional>

class KX_Scene;
class KX_Camera;

/** \brief This file contains all the data describing the rendering proceeded in a frame.
 * KX_RenderData is the main data which for each eye (in case of stereo) contains a frame
 * and each of these frame contains the scenes data and cameras data.
 */

// Storing information for off screen rendering of shadow and texture map.
struct KX_TextureRenderData
{
	enum Mode {
		MODE_NONE = 0,
		MODE_RENDER_WORLD = (1 << 0),
		MODE_UPDATE_LOD = (1 << 1)
	} m_mode;

	RAS_Rasterizer::ClearBit m_clearMode;
	RAS_Rasterizer::DrawType m_drawingMode;

	mt::mat4 m_viewMatrix;
	mt::mat4 m_progMatrix;
	mt::mat3x4 m_camTrans;
	mt::vec3 m_position;

	SG_Frustum m_frustum;
	unsigned int m_cullingLayer;

	float m_lodFactor;

	unsigned short m_index;

	std::function<void (RAS_Rasterizer *)> m_bind;
	std::function<void (RAS_Rasterizer *)> m_unbind;
};

using KX_TextureRenderDataList = std::vector<KX_TextureRenderData>;

struct KX_CameraRenderData
{
	mt::mat4 m_viewMatrix;
	mt::mat4 m_progMatrix;
	mt::mat3x4 m_camTrans;
	mt::vec3 m_position;
	bool m_negScale;
	bool m_perspective;

	RAS_FrameFrustum m_frameFrustum;
	SG_Frustum m_frustum;
	bool m_culling;

	RAS_Rect m_area;
	RAS_Rect m_viewport;

	float m_lodFactor;

	RAS_Rasterizer::StereoMode m_stereoMode;
	RAS_Rasterizer::StereoEye m_eye;
	float m_focalLength;
	// Index of the camera in all the scene's cameras rendered.
	unsigned short m_index;
};

using KX_CameraRenderDataList = std::vector<KX_CameraRenderData>;

struct KX_SceneRenderData
{
	KX_Scene *m_scene;
	KX_TextureRenderDataList m_textureDataList;
	// Use multiple list of cameras in case of per eye stereo.
	KX_CameraRenderDataList m_cameraDataList[RAS_Rasterizer::RAS_STEREO_MAXEYE];
};

using KX_SceneRenderDataList = std::vector<KX_SceneRenderData>;

/// Data used to render a frame.
struct KX_FrameRenderData
{	
	RAS_Rasterizer::OffScreenType m_ofsType;
	std::vector<RAS_Rasterizer::StereoEye> m_eyes;
};

using KX_FrameRenderDataList = std::vector<KX_FrameRenderData>;

struct KX_RenderData
{
	RAS_FrameSettings m_frameSettings;
	RAS_Rasterizer::StereoMode m_stereoMode;
	bool m_renderPerEye;
	KX_SceneRenderDataList m_sceneDataList;
	KX_FrameRenderDataList m_frameDataList;
};

#endif  // __KX_RENDER_DATA_H__
