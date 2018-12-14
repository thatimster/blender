/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Ulysse Martin, Tristan Porteries, Martins Upitis.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file gameengine/Ketsji/KX_TextureRendererManager.cpp
 *  \ingroup ketsji
 */

#include "KX_TextureRendererManager.h"
#include "KX_Camera.h"
#include "KX_Scene.h"
#include "KX_Globals.h"
#include "KX_CubeMap.h"
#include "KX_PlanarMap.h"

#include "RAS_Rasterizer.h"
#include "RAS_OffScreen.h"
#include "RAS_Texture.h"

#include "DNA_texture_types.h"

#include "CM_Message.h"

KX_TextureRendererManager::KX_TextureRendererManager()
{
}

KX_TextureRendererManager::~KX_TextureRendererManager()
{
	for (KX_TextureRenderer *renderer : m_renderers) {
		delete renderer;
	}
}

void KX_TextureRendererManager::InvalidateViewpoint(KX_GameObject *gameobj)
{
	for (KX_TextureRenderer *renderer : m_renderers) {
		if (renderer->GetViewpointObject() == gameobj) {
			renderer->SetViewpointObject(nullptr);
		}
	}
}

void KX_TextureRendererManager::ReloadTextures()
{
	for (KX_TextureRenderer *renderer : m_renderers) {
		renderer->ReloadTexture();
	}
}

void KX_TextureRendererManager::AddRenderer(RendererType type, RAS_Texture *texture, KX_GameObject *viewpoint)
{
	// Find a shared renderer (using the same material texture) or create a new one.
	for (KX_TextureRenderer *renderer : m_renderers) {
		if (texture->GetMTex() == renderer->GetMTex()) {
			texture->SetRenderer(renderer);
			KX_GameObject *origviewpoint = renderer->GetViewpointObject();
			if (viewpoint != origviewpoint) {
				CM_Warning("texture renderer (" << texture->GetName() << ") uses different viewpoint objects (" <<
				           (origviewpoint ? origviewpoint->GetName() : "<None>") << " and " << viewpoint->GetName() << ").");
			}
			return;
		}
	}

	MTex *mtex = texture->GetMTex();
	KX_TextureRenderer *renderer;
	switch (type) {
		case CUBE:
		{
			renderer = new KX_CubeMap(mtex, viewpoint);
			break;
		}
		case PLANAR:
		{
			renderer = new KX_PlanarMap(mtex, viewpoint);
			break;
		}
	}

	texture->SetRenderer(renderer);
	m_renderers.push_back(renderer);
}

KX_TextureRenderScheduleList KX_TextureRendererManager::ScheduleRenderer(RAS_Rasterizer *rasty, KX_TextureRenderer *renderer,
		const std::vector<const KX_CameraRenderSchedule *>& cameraSchedules)
{
	KX_GameObject *viewpoint = renderer->GetViewpointObject();
	// Doesn't need (or can) update.
	if (!renderer->NeedUpdate() || !renderer->GetEnabled() || !viewpoint) {
		return {};
	}

	const int visibleLayers = ~renderer->GetIgnoreLayers();
	const float lodFactor = renderer->GetLodDistanceFactor();

	const bool visible = viewpoint->GetVisible();

	// Ensure the number of layers for all viewports or use a unique layer.
	const unsigned short numViewport = cameraSchedules.size();
	RAS_TextureRenderer::LayerUsage usage = renderer->EnsureLayers(numViewport);
	const unsigned short numlay = (usage == RAS_TextureRenderer::LAYER_SHARED) ? 1 : numViewport;

	KX_TextureRenderScheduleList textures;

	for (unsigned short layer = 0; layer < numlay; ++layer) {
		/* Two cases are possible :
		 * - Only one layer is present for any number of viewports,
		 *   in this case the renderer must not care about the viewport (e.g cube map).
		 * - Multiple layers are present, one per viewport, in this case
		 *   the renderer could care of the viewport and the index of the layer
		 *   match the index of the viewport in the scene.
		 */

		const KX_CameraRenderSchedule *cameraSchedule = cameraSchedules[layer];

		/* When we update clipstart or clipend values,
		* or if the projection matrix is not computed yet,
		* we have to compute projection matrix.
		*/
		const mt::mat4 projmat = renderer->GetProjectionMatrix(rasty, *cameraSchedule);

		for (unsigned short face = 0, numface = renderer->GetNumFaces(layer); face < numface; ++face) {
			mt::mat3x4 camtrans;
			// Set camera settings unique per faces.
			if (!renderer->PrepareFace(cameraSchedule->m_viewMatrix, face, camtrans)) {
				continue;
			}

			const mt::mat4 viewmat = mt::mat4::FromAffineTransform(camtrans).Inverse();
			const SG_Frustum frustum(projmat * viewmat);

			KX_TextureRenderSchedule textureSchedule;
			textureSchedule.m_mode = (KX_TextureRenderSchedule::Mode)(KX_TextureRenderSchedule::MODE_RENDER_WORLD | KX_TextureRenderSchedule::MODE_UPDATE_LOD);
			textureSchedule.m_clearMode = 
					(RAS_Rasterizer::ClearBit)(RAS_Rasterizer::RAS_DEPTH_BUFFER_BIT | RAS_Rasterizer::RAS_COLOR_BUFFER_BIT);
			textureSchedule.m_drawingMode = RAS_Rasterizer::RAS_RENDERER;
			textureSchedule.m_viewMatrix = viewmat;
			textureSchedule.m_progMatrix = projmat;
			textureSchedule.m_camTrans = camtrans;
			textureSchedule.m_position = camtrans.TranslationVector3D();
			textureSchedule.m_frustum = frustum;
			textureSchedule.m_visibleLayers = visibleLayers;
			textureSchedule.m_lodFactor = lodFactor;
			textureSchedule.m_eye = cameraSchedule->m_eye;
			textureSchedule.m_index = layer;
			textureSchedule.m_bind = [renderer, viewpoint, layer, face](RAS_Rasterizer *rasty){
				/* We hide the viewpoint object in the case backface culling is disabled -> we can't see through
				 * the object faces if the camera is inside the gameobject.
				 */
				viewpoint->SetVisible(false, false);
				renderer->BeginRenderFace(rasty, layer, face);
			};
			textureSchedule.m_unbind = [renderer, viewpoint, visible, layer, face](RAS_Rasterizer *rasty){
				renderer->EndRenderFace(rasty, layer, face);
				viewpoint->SetVisible(visible, false);
			};

			textures.push_back(textureSchedule);
		}
	}

	return textures;
}

KX_TextureRenderScheduleList KX_TextureRendererManager::ScheduleRender(RAS_Rasterizer *rasty, const KX_SceneRenderSchedule& sceneSchedule)
{
	if (m_renderers.empty()) {
		return {};
	}

	// Get the number of viewports.
	const unsigned short viewportCount = sceneSchedule.m_cameraScheduleList[RAS_Rasterizer::RAS_STEREO_LEFTEYE].size() +
	sceneSchedule.m_cameraScheduleList[RAS_Rasterizer::RAS_STEREO_RIGHTEYE].size();
	// Construct a list of all the camera data by the viewport index order.
	std::vector<const KX_CameraRenderSchedule *> cameraSchedules(viewportCount);
	for (unsigned short eye = RAS_Rasterizer::RAS_STEREO_LEFTEYE; eye < RAS_Rasterizer::RAS_STEREO_MAXEYE; ++eye) {
		for (const KX_CameraRenderSchedule& cameraSchedule : sceneSchedule.m_cameraScheduleList[eye]) {
			cameraSchedules[cameraSchedule.m_index] = &cameraSchedule;
		}
	}

	KX_TextureRenderScheduleList allTextures;
	for (KX_TextureRenderer *renderer : m_renderers) {
		const KX_TextureRenderScheduleList textures = ScheduleRenderer(rasty, renderer, cameraSchedules);
		allTextures.insert(allTextures.end(), textures.begin(), textures.end());
	}

	return allTextures;
}

void KX_TextureRendererManager::Merge(KX_TextureRendererManager *other)
{
	m_renderers.insert(m_renderers.end(), other->m_renderers.begin(), other->m_renderers.end());
	other->m_renderers.clear();
}
