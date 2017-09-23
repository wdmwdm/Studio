/**************************************************************************
This file is part of JahshakaVR, VR Authoring Toolkit
http://www.jahshaka.com
Copyright (c) 2016  GPLv3 Jahshaka LLC <coders@jahshaka.com>

This is free software: you may copy, redistribute
and/or modify it under the terms of the GPLv3 License

For more information see the LICENSE file
*************************************************************************/

#include "sceneviewwidget.h"
#include "../constants.h"
#include <QTimer>

#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QMouseEvent>
#include <QtMath>
#include <QDebug>
#include <QMimeData>
#include <QElapsedTimer>

#include "../irisgl/src/irisgl.h"
#include "../irisgl/src/scenegraph/scenenode.h"
#include "../irisgl/src/scenegraph/meshnode.h"
#include "../irisgl/src/scenegraph/cameranode.h"
#include "../irisgl/src/scenegraph/lightnode.h"
#include "../irisgl/src/scenegraph/viewernode.h"
#include "../irisgl/src/materials/defaultmaterial.h"
#include "../irisgl/src/graphics/forwardrenderer.h"
#include "../irisgl/src/graphics/mesh.h"
#include "../irisgl/src/geometry/trimesh.h"
#include "../irisgl/src/graphics/texture2d.h"
#include "../irisgl/src/graphics/viewport.h"
#include "../irisgl/src/graphics/renderlist.h"
#include "../irisgl/src/graphics/rendertarget.h"
#include "../irisgl/src/graphics/font.h"
#include "../irisgl/src/graphics/spritebatch.h"
#include "../irisgl/src/graphics/utils/fullscreenquad.h"
#include "../irisgl/src/vr/vrmanager.h"
#include "../irisgl/src/vr/vrdevice.h"

#include "../editor/cameracontrollerbase.h"
#include "../editor/editorcameracontroller.h"
#include "../editor/orbitalcameracontroller.h"
#include "../editor/editorvrcontroller.h"

#include "../editor/editordata.h"

#include "../editor/translationgizmo.h"
#include "../editor/rotationgizmo.h"
#include "../editor/scalegizmo.h"

#include "../core/keyboardstate.h"

#include "../irisgl/src/graphics/utils/shapehelper.h"
#include "../irisgl/src/graphics/utils/linemeshbuilder.h"
#include "../irisgl/src/materials/colormaterial.h"

#include "../editor/thumbnailgenerator.h"

SceneViewWidget::SceneViewWidget(QWidget *parent) : QOpenGLWidget(parent)
{
    QSurfaceFormat format;
    format.setDepthBufferSize(32);
    format.setMajorVersion(3);
    format.setMinorVersion(2);
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setSamples(1);
    setFormat(format);

    // needed in order to get mouse events
    setMouseTracking(true);

    // needed in order to get key events http://stackoverflow.com/a/7879484/991834
    setFocusPolicy(Qt::ClickFocus);
    setAcceptDrops(true);

    viewport = new iris::Viewport();

    defaultCam = new EditorCameraController();
    orbitalCam = new OrbitalCameraController();

    camController = defaultCam;
    prevCamController = defaultCam;

    editorCam = iris::CameraNode::create();
    editorCam->setLocalPos(QVector3D(0, 5, 14));
    editorCam->setLocalRot(QQuaternion::fromEulerAngles(-5, 0, 0));
    camController->setCamera(editorCam);

    viewportMode = ViewportMode::Editor;

    elapsedTimer = new QElapsedTimer();
    playScene = false;
    animTime = 0.0f;

    dragging = false;
    showLightWires = false;

    sceneFloor = iris::IntersectionHelper::computePlaneND(QVector3D( 100, 0,  100),
                                                          QVector3D(-100, 0,  100),
                                                          QVector3D(   0, 0, -100));

    setAcceptDrops(true);
    thumbGen = nullptr;

    fontSize = 20;
}

void SceneViewWidget::resetEditorCam()
{
    editorCam->setLocalPos(QVector3D(0, 5, 14));
    editorCam->setLocalRot(QQuaternion::fromEulerAngles(-5, 0, 0));
    camController->setCamera(editorCam);
}

void SceneViewWidget::initialize()
{
    initialH = true;

    translationGizmo = new TranslationGizmo(editorCam);
    translationGizmo->createHandleShader();

    rotationGizmo = new RotationGizmo(editorCam);
    rotationGizmo->createHandleShader();

    scaleGizmo = new ScaleGizmo(editorCam);
    scaleGizmo->createHandleShader();

    viewportGizmo = translationGizmo;
    transformMode = "Global";

    // has to be initialized here since it loads assets
    vrCam = new EditorVrController();

    initLightAssets();
}

bool SceneViewWidget::getShowLightWires() const
{
    return showLightWires;
}

void SceneViewWidget::setShowLightWires(bool value)
{
    showLightWires = value;
}

void SceneViewWidget::initLightAssets()
{
    pointLightMesh = iris::ShapeHelper::createWireSphere(1.0f);
    spotLightMesh = iris::ShapeHelper::createWireCone(1.0f);
    dirLightMesh = createDirLightMesh();
    lineMat = iris::ColorMaterial::create();
}

iris::MeshPtr SceneViewWidget::createDirLightMesh(float baseRadius)
{
    iris::LineMeshBuilder builder;

    int divisions = 36;
    float arcWidth = 360.0f/divisions;

    // XZ plane
    for (int i = 0 ; i < divisions; i++) {
        float angle = i * arcWidth;
        QVector3D a = QVector3D(qSin(qDegreesToRadians(angle)), 0, qCos(qDegreesToRadians(angle))) * baseRadius;

        angle = (i + 1) * arcWidth;
        QVector3D b = QVector3D(qSin(qDegreesToRadians(angle)), 0, qCos(qDegreesToRadians(angle))) * baseRadius;

        builder.addLine(a, b);
    }

    float halfSize = 0.5f;

    builder.addLine(QVector3D(-halfSize, 0, -halfSize), QVector3D(-halfSize, -2, -halfSize));
    builder.addLine(QVector3D(halfSize, 0, -halfSize), QVector3D(halfSize, -2, -halfSize));
    builder.addLine(QVector3D(halfSize, 0, halfSize), QVector3D(halfSize, -2, halfSize));
    builder.addLine(QVector3D(-halfSize, 0, halfSize), QVector3D(-halfSize, -2, halfSize));

    return builder.build();
}

void SceneViewWidget::addLightShapesToScene()
{
    QMatrix4x4 mat;

    for (auto light : scene->lights) {
        if (light->lightType == iris::LightType::Point) {
            mat.setToIdentity();
            mat.translate(light->getGlobalPosition());
            mat.scale(light->distance);
            scene->geometryRenderList->submitMesh(pointLightMesh, lineMat, mat);
        }
        else if (light->lightType == iris::LightType::Spot) {
            mat.setToIdentity();
            mat.translate(light->getGlobalPosition());
            mat.rotate(QQuaternion::fromRotationMatrix(light->getGlobalTransform().normalMatrix()));
            auto radius = qCos(qDegreesToRadians(light->spotCutOff - 90)); // 90 is max spot cutoff (180) / 2

            mat.scale(radius, 1.0 - radius, radius);
            mat.scale(light->distance);

            scene->geometryRenderList->submitMesh(spotLightMesh, lineMat, mat);
        }
        else if (light->lightType == iris::LightType::Directional) {
            mat.setToIdentity();
            mat.translate(light->getGlobalPosition());
            mat.rotate(QQuaternion::fromRotationMatrix(light->getGlobalTransform().normalMatrix()));

            scene->geometryRenderList->submitMesh(dirLightMesh, lineMat, mat);
        }
    }
}

void SceneViewWidget::setScene(iris::ScenePtr scene)
{
    this->scene = scene;
    scene->setCamera(editorCam);
    renderer->setScene(scene);
    vrCam->setScene(scene);

    // remove selected scenenode
    selectedNode.reset();
}

void SceneViewWidget::setSelectedNode(iris::SceneNodePtr sceneNode)
{
    selectedNode = sceneNode;
    renderer->setSelectedSceneNode(sceneNode);

    if (viewportGizmo != nullptr) {
        viewportGizmo->setLastSelectedNode(sceneNode);
    }
}

void SceneViewWidget::clearSelectedNode()
{
    selectedNode.clear();
    renderer->setSelectedSceneNode(selectedNode);
}

void SceneViewWidget::updateScene(bool once)
{
    // update and draw the 3d manipulation gizmo
    if (!!viewportGizmo->getLastSelectedNode()) {
        if (!viewportGizmo->getLastSelectedNode()->isRootNode()) {
            viewportGizmo->updateTransforms(editorCam->getGlobalPosition());
            if (viewportMode != ViewportMode::VR && UiManager::sceneMode == SceneMode::EditMode) {
                viewportGizmo->render(editorCam->viewMatrix, editorCam->projMatrix);
            }
        }
    }
}

void SceneViewWidget::initializeGL()
{
    QOpenGLWidget::initializeGL();

    makeCurrent();

    initializeOpenGLFunctions();

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    renderer = iris::ForwardRenderer::create();
    spriteBatch = iris::SpriteBatch::create(renderer->getGraphicsDevice());
    font = iris::Font::create(renderer->getGraphicsDevice(), fontSize);

    initialize();
    fsQuad = new iris::FullScreenQuad();

    viewerCamera = iris::CameraNode::create();
    viewerRT = iris::RenderTarget::create(500, 500);
    viewerTex = iris::Texture2D::create(500, 500);
    viewerRT->addTexture(viewerTex);
    viewerQuad = new iris::FullScreenQuad();

    screenshotRT = iris::RenderTarget::create(500, 500);
    screenshotTex = iris::Texture2D::create(500, 500);
    screenshotRT->addTexture(screenshotTex);

    emit initializeGraphics(this, this);

    //thumbGen = new ThumbnialGenerator();
    thumbGen = ThumbnailGenerator::getSingleton();

    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(update()));
    timer->start(Constants::FPS_60);

    this->elapsedTimer->start();
}

void SceneViewWidget::paintGL()
{
    makeCurrent();

    if (iris::VrManager::getDefaultDevice()->isHeadMounted() && viewportMode != ViewportMode::VR) {
        // set to vr mode automatically if a headset is detected
        this->setViewportMode(ViewportMode::VR);
        timer->setInterval(Constants::FPS_90); // 90 fps for vr
    }
    else if (!iris::VrManager::getDefaultDevice()->isHeadMounted() &&
            viewportMode == ViewportMode::VR)
    {
        this->setViewportMode(ViewportMode::Editor);
        timer->setInterval(Constants::FPS_60); // 60 for regular
    }

    renderScene();

}

void SceneViewWidget::renderScene()
{
    glClearColor(.1f, .1f, .1f, .4f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    float dt = elapsedTimer->nsecsElapsed() / (1000.0f * 1000.0f * 1000.0f);
    elapsedTimer->restart();

    if (!!renderer && !!scene) {
        this->camController->update(dt);

        if (playScene) {
            animTime += dt;
            scene->updateSceneAnimation(animTime);
        }

        // hide viewer so it doesnt show up in rt
        bool viewerVisible = true;

        if (!!selectedNode && selectedNode->getSceneNodeType() == iris::SceneNodeType::Viewer) {
            viewerVisible = selectedNode->isVisible();
            selectedNode->hide();
        }

        scene->update(dt);

        // insert vr head
        if (UiManager::sceneMode != SceneMode::PlayMode || viewportMode != ViewportMode::VR) {
            for (auto view : scene->viewers)
                view->submitRenderItems();
        }

        // TODO: ensure it doesnt display these shapes in play mode (Nick)
        if (UiManager::sceneMode != SceneMode::PlayMode && showLightWires) addLightShapesToScene();

        // render thumbnail to texture
        if (!!selectedNode && selectedNode->getSceneNodeType() == iris::SceneNodeType::Viewer) {
            viewerCamera->setLocalTransform(selectedNode->getGlobalTransform());
            viewerCamera->update(0); // update transformation of camera

            // resize render target to fix aspect ratio
            viewerRT->resize(this->width(), this->height(), true);

            renderer->renderSceneToRenderTarget(viewerRT, viewerCamera);

            // restore viewer visibility state
            if (viewerVisible) {
                selectedNode->show();

                // let it show back in regular scene rendering mode
                // i know this looks like a hack, but it'll
                // have to do until we find a better way to do this
                selectedNode->submitRenderItems();
            }
        }

        if (viewportMode == ViewportMode::Editor) {
            renderer->renderScene(dt, viewport);
        } else {
            renderer->renderSceneVr(dt, viewport, UiManager::sceneMode == SceneMode::PlayMode);
        }

        if (!!selectedNode && selectedNode->getSceneNodeType() == iris::SceneNodeType::Viewer) {
            QOpenGLContext::currentContext()->versionFunctions<QOpenGLFunctions_3_2_Core>()->glClear(GL_DEPTH_BUFFER_BIT);
            QMatrix4x4 mat;
            mat.setToIdentity();
            mat.translate(0.75, -0.75, 0);
            mat.scale(0.2, 0.2, 0);
            viewerTex->bind(0);
            viewerQuad->matrix = mat;
            viewerQuad->draw();
        }

        this->updateScene();
    }

    // render fps
    float fps = 1.0 / dt;
    float ms = 1000.f / fps;
    spriteBatch->begin();
    spriteBatch->drawString(font,
                            QString("%1ms (%2fps)")
                                .arg(QString::number(ms, 'f', 1))
                                .arg(QString::number(fps, 'f', 1)),
                            QVector2D(8, 8),
                            QColor(255, 255, 255));
    spriteBatch->end();

}

void SceneViewWidget::resizeGL(int width, int height)
{
    // we do an explicit call to glViewport(...) in forwardrenderer
    // with the "good DPI" values so it is not needed here initially (iKlsR)
    viewport->pixelRatioScale = devicePixelRatio();
    viewport->width = width;
    viewport->height = height;
}

bool SceneViewWidget::eventFilter(QObject *obj, QEvent *event)
{
    return QWidget::eventFilter(obj, event);
}

QVector3D SceneViewWidget::calculateMouseRay(const QPointF& pos)
{
    float x = pos.x();
    float y = pos.y();

    // viewport -> NDC
    float mousex = (2.0f * x) / this->viewport->width - 1.0f;
    float mousey = (2.0f * y) / this->viewport->height - 1.0f;
    QVector2D NDC = QVector2D(mousex, -mousey);

    // NDC -> HCC
    QVector4D HCC = QVector4D(NDC, -1.0f, 1.0f);

    // HCC -> View Space
    QMatrix4x4 projection_matrix_inverse = this->editorCam->projMatrix.inverted();
    QVector4D eye_coords = projection_matrix_inverse * HCC;
    QVector4D ray_eye = QVector4D(eye_coords.x(), eye_coords.y(), -1.0f, 0.0f);

    // View Space -> World Space
    QMatrix4x4 view_matrix_inverse = this->editorCam->viewMatrix.inverted();
    QVector4D world_coords = view_matrix_inverse * ray_eye;
    QVector3D final_ray_coords = QVector3D(world_coords);

    return final_ray_coords.normalized();
}

bool SceneViewWidget::updateRPI(QVector3D pos, QVector3D r) {
    float t;
    QVector3D q;

    if (iris::IntersectionHelper::intersectSegmentPlane(pos, (r * 1024), sceneFloor, t, q)) {
        Offset = q;
        return true;
    }

    return false;
}

bool SceneViewWidget::doActiveObjectPicking(const QPointF &point)
{
    editorCam->updateCameraMatrices();

    auto segStart = this->editorCam->getLocalPos();
    auto rayDir = this->calculateMouseRay(point) * 1024;
    auto segEnd = segStart + rayDir;

    QList<PickingResult> hitList;
    doScenePicking(scene->getRootNode(), segStart, segEnd, hitList);

    if (hitList.size() == 0) return false;

    if (hitList.size() == 1) {
        hit = hitList.last().hitPoint;
        return true;
    }

    qSort(hitList.begin(), hitList.end(), [](const PickingResult& a, const PickingResult& b) {
        return a.distanceFromCameraSqrd > b.distanceFromCameraSqrd;
    });

    hit = hitList.last().hitPoint;
    return true;
}

void SceneViewWidget::mouseMoveEvent(QMouseEvent *e)
{
    // @ISSUE - only fired when mouse is dragged
    QPointF localPos = e->localPos();
    QPointF dir = localPos - prevMousePos;

    if (e->buttons() == Qt::LeftButton && !!viewportGizmo->currentNode) {
         viewportGizmo->update(editorCam->getLocalPos(), calculateMouseRay(localPos));
    }

    if (camController != nullptr) {
        camController->onMouseMove(-dir.x(), -dir.y());
    }

    prevMousePos = localPos;
}

void SceneViewWidget::mousePressEvent(QMouseEvent *e)
{
    auto lastSelected = selectedNode;
    prevMousePos = e->localPos();

    if (e->button() == Qt::RightButton) {
        dragging = true;
    }

    if (e->button() == Qt::LeftButton) {
        editorCam->updateCameraMatrices();

        if (viewportMode == ViewportMode::Editor) {
            this->doGizmoPicking(e->localPos());

            if (!!selectedNode) {
                viewportGizmo->isGizmoHit(editorCam, e->localPos(), this->calculateMouseRay(e->localPos()));
                viewportGizmo->isHandleHit();
            }

            // if we don't have a selected node prioritize object picking
            if (selectedNode.isNull()) {
                this->doObjectPicking(e->localPos(), lastSelected);
            }
        }
    }

    if (camController != nullptr) {
        camController->onMouseDown(e->button());
    }
}

void SceneViewWidget::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::RightButton) {
        dragging = false;
    }

    if (e->button() == Qt::LeftButton) {
        // maybe explicitly hard reset stuff related to picking here
        viewportGizmo->onMouseRelease();
    }

    if (camController != nullptr) {
        camController->onMouseUp(e->button());
    }
}

void SceneViewWidget::wheelEvent(QWheelEvent *event)
{
    if (camController != nullptr) {
        camController->onMouseWheel(event->delta());
    }
}

void SceneViewWidget::keyPressEvent(QKeyEvent *event)
{
    KeyboardState::keyStates[event->key()] = true;
}

void SceneViewWidget::keyReleaseEvent(QKeyEvent *event)
{
    KeyboardState::keyStates[event->key()] = false;
}

void SceneViewWidget::focusOutEvent(QFocusEvent* event)
{
    KeyboardState::reset();
}

/*
 * if @selectRootObject is false then it will return the exact picked object
 * else it will compared the roots of the current and previously selected objects
 * @selectRootObject is usually true for picking using the mouse
 * It's false for when dragging a texture to an object
 */
void SceneViewWidget::doObjectPicking(const QPointF& point, iris::SceneNodePtr lastSelectedNode, bool selectRootObject, bool skipLights, bool skipViewers)
{
    editorCam->updateCameraMatrices();

    auto segStart = this->editorCam->getLocalPos();
    auto rayDir = this->calculateMouseRay(point) * 1024;
    auto segEnd = segStart + rayDir;

    QList<PickingResult> hitList;
    doScenePicking(scene->getRootNode(), segStart, segEnd, hitList);
    if (!skipLights) {
        doLightPicking(segStart, segEnd, hitList);
    }

    if (!skipViewers) {
        doViewerPicking(segStart, segEnd, hitList);
    }

    if (hitList.size() == 0) {
        // no hits, deselect last selected object in viewport and heirarchy
        emit sceneNodeSelected(iris::SceneNodePtr());
        return;
    }

    // sort by distance to camera then return the closest hit node
    qSort(hitList.begin(), hitList.end(), [](const PickingResult& a, const PickingResult& b) {
        return a.distanceFromCameraSqrd > b.distanceFromCameraSqrd;
    });

    auto pickedNode = hitList.last().hitNode;
    iris::SceneNodePtr lastSelectedRoot;

    if (selectRootObject) {
        if (!!lastSelectedNode) {
            lastSelectedRoot = lastSelectedNode;
            while (lastSelectedRoot->isAttached())
                lastSelectedRoot = lastSelectedRoot->parent;
        }

        auto pickedRoot = hitList.last().hitNode;
        while (pickedRoot->isAttached())
            pickedRoot = pickedRoot->parent;



        if (!lastSelectedNode ||            // if the user clicked away then the root should be reselected
            pickedRoot != lastSelectedRoot) // if both are under, or is, the same root then pick the actual object
            pickedNode = pickedRoot;        // if not then pick the root node
    }

    viewportGizmo->setLastSelectedNode(pickedNode);
    emit sceneNodeSelected(pickedNode);
}

QImage SceneViewWidget::takeScreenshot(int width, int height)
{
    this->makeCurrent();
    screenshotRT->resize(width, height, true);
    scene->update(0);
    renderer->renderSceneToRenderTarget(screenshotRT, editorCam, false);

    auto img = screenshotRT->toImage();
    this->doneCurrent();

    return img;
}

void SceneViewWidget::doGizmoPicking(const QPointF& point)
{
    editorCam->updateCameraMatrices();

    auto segStart = this->editorCam->getLocalPos();
    auto rayDir = this->calculateMouseRay(point) * 1024;
    auto segEnd = segStart + rayDir;

    QList<PickingResult> hitList;
    doMeshPicking(viewportGizmo->getRootNode(), segStart, segEnd, hitList);

    if (hitList.size() == 0) {
        viewportGizmo->setLastSelectedNode(iris::SceneNodePtr());
        viewportGizmo->currentNode = iris::SceneNodePtr();
        emit sceneNodeSelected(iris::SceneNodePtr());
        return;
    }

    qSort(hitList.begin(), hitList.end(), [](const PickingResult& a, const PickingResult& b) {
        return a.distanceFromCameraSqrd > b.distanceFromCameraSqrd;
    });

    viewportGizmo->finalHitPoint = hitList.last().hitPoint;
    viewportGizmo->setPlaneOrientation(hitList.last().hitNode->getName());
    viewportGizmo->currentNode = hitList.last().hitNode;
    viewportGizmo->onMousePress(editorCam->getLocalPos(), this->calculateMouseRay(point) * 1024);
}

void SceneViewWidget::doScenePicking(const QSharedPointer<iris::SceneNode>& sceneNode,
                                     const QVector3D& segStart,
                                     const QVector3D& segEnd,
                                     QList<PickingResult>& hitList)
{
    if ((sceneNode->getSceneNodeType() == iris::SceneNodeType::Mesh) &&
         sceneNode->isPickable())
    {
        auto meshNode = sceneNode.staticCast<iris::MeshNode>();
        auto mesh = meshNode->getMesh();
        if (mesh != nullptr) {
            auto triMesh = meshNode->getMesh()->getTriMesh();

            // transform segment to local space
            auto invTransform = meshNode->globalTransform.inverted();
            auto a = invTransform * segStart;
            auto b = invTransform * segEnd;

            QList<iris::TriangleIntersectionResult> results;
            if (int resultCount = triMesh->getSegmentIntersections(a, b, results)) {
                for (auto triResult : results) {
                    // convert hit to world space
                    auto hitPoint = meshNode->globalTransform * triResult.hitPoint;

                    PickingResult pick;
                    pick.hitNode = sceneNode;
                    pick.hitPoint = hitPoint;
                    pick.distanceFromCameraSqrd = (hitPoint - editorCam->getGlobalPosition()).lengthSquared();

                    hitList.append(pick);
                }
            }
        }
    }

    for (auto child : sceneNode->children) {
        doScenePicking(child, segStart, segEnd, hitList);
    }
}

void SceneViewWidget::doMeshPicking(const QSharedPointer<iris::SceneNode>& sceneNode,
                                    const QVector3D& segStart,
                                    const QVector3D& segEnd,
                                    QList<PickingResult>& hitList)
{
    if (sceneNode->getSceneNodeType() == iris::SceneNodeType::Mesh) {
        auto meshNode = sceneNode.staticCast<iris::MeshNode>();
        auto triMesh = meshNode->getMesh()->getTriMesh();

        // transform segment to local space
        auto invTransform = meshNode->globalTransform.inverted();
        auto a = invTransform * segStart;
        auto b = invTransform * segEnd;

        QList<iris::TriangleIntersectionResult> results;
        if (int resultCount = triMesh->getSegmentIntersections(a, b, results)) {
            for (auto triResult : results) {
                // convert hit to world space
                auto hitPoint = meshNode->globalTransform * triResult.hitPoint;

                PickingResult pick;
                pick.hitNode = sceneNode;
                pick.hitPoint = hitPoint;
                pick.distanceFromCameraSqrd = (hitPoint - editorCam->getGlobalPosition()).lengthSquared();

                hitList.append(pick);
            }
        }
    }

    for (auto child : sceneNode->children) {
        doMeshPicking(child, segStart, segEnd, hitList);
    }
}

void SceneViewWidget::doLightPicking(const QVector3D& segStart,
                                     const QVector3D& segEnd,
                                     QList<PickingResult>& hitList)
{
    const float lightRadius = 0.5f;

    auto rayDir = (segEnd-segStart);
    float segLengthSqrd = rayDir.lengthSquared();
    rayDir.normalize();
    QVector3D hitPoint;
    float t;

    for (auto light: scene->lights) {
        if (iris::IntersectionHelper::raySphereIntersects(segStart,
                                                          rayDir,
                                                          light->getLocalPos(),
                                                          lightRadius,
                                                          t, hitPoint))
        {
            PickingResult pick;
            pick.hitNode = light.staticCast<iris::SceneNode>();
            pick.hitPoint = hitPoint;
            pick.distanceFromCameraSqrd = (hitPoint-editorCam->getGlobalPosition()).lengthSquared();

            hitList.append(pick);
        }
    }
}

void SceneViewWidget::doViewerPicking(const QVector3D& segStart,
                                     const QVector3D& segEnd,
                                     QList<PickingResult>& hitList)
{
    const float headRadius = 0.5f;

    auto rayDir = (segEnd-segStart);
    float segLengthSqrd = rayDir.lengthSquared();
    rayDir.normalize();
    QVector3D hitPoint;
    float t;

    for (auto viewer: scene->viewers) {
        if (iris::IntersectionHelper::raySphereIntersects(segStart,
                                                          rayDir,
                                                          viewer->getGlobalPosition(),
                                                          headRadius,
                                                          t, hitPoint))
        {
            PickingResult pick;
            pick.hitNode = viewer.staticCast<iris::SceneNode>();
            pick.hitPoint = hitPoint;
            pick.distanceFromCameraSqrd = (hitPoint-editorCam->getGlobalPosition()).lengthSquared();

            hitList.append(pick);
        }
    }
}

void SceneViewWidget::setFreeCameraMode()
{
    camController = defaultCam;
    camController->setCamera(editorCam);
    camController->resetMouseStates();
}

void SceneViewWidget::setArcBallCameraMode()
{
    camController = orbitalCam;
    camController->setCamera(editorCam);
    camController->resetMouseStates();
}

bool SceneViewWidget::isVrSupported()
{
    return renderer->isVrSupported();
}

void SceneViewWidget::setViewportMode(ViewportMode viewportMode)
{
    this->viewportMode = viewportMode;

    // switch cam to vr mode
    if (viewportMode == ViewportMode::VR) {
        prevCamController = camController;
        camController = vrCam;
        camController->setCamera(editorCam);
        camController->resetMouseStates();
    }
    else {
        camController = prevCamController;
        camController->setCamera(editorCam);
        camController->resetMouseStates();
    }
}

ViewportMode SceneViewWidget::getViewportMode()
{
    return viewportMode;
}

void SceneViewWidget::setTransformOrientationLocal()
{
    viewportGizmo->setTransformOrientation("Local");
}

void SceneViewWidget::setTransformOrientationGlobal()
{
    viewportGizmo->setTransformOrientation("Global");
}

void SceneViewWidget::hideGizmo()
{
    viewportGizmo->setLastSelectedNode(iris::SceneNodePtr());
}

void SceneViewWidget::setGizmoLoc()
{
    editorCam->updateCameraMatrices();
    transformMode = viewportGizmo->getTransformOrientation();
    viewportGizmo = translationGizmo;
    viewportGizmo->setTransformOrientation(transformMode);
    viewportGizmo->setLastSelectedNode(selectedNode);
}

void SceneViewWidget::setGizmoRot()
{
    editorCam->updateCameraMatrices();
    transformMode = viewportGizmo->getTransformOrientation();
    viewportGizmo = rotationGizmo;
    viewportGizmo->setTransformOrientation(transformMode);
    viewportGizmo->setLastSelectedNode(selectedNode);
}

void SceneViewWidget::setGizmoScale()
{
    editorCam->updateCameraMatrices();
    transformMode = viewportGizmo->getTransformOrientation();
    viewportGizmo = scaleGizmo;
    viewportGizmo->setTransformOrientation(transformMode);
    viewportGizmo->setLastSelectedNode(selectedNode);
}

void SceneViewWidget::setEditorData(EditorData* data)
{
    editorCam = data->editorCamera;
    orbitalCam->distFromPivot = data->distFromPivot;
    scene->setCamera(editorCam);
    camController->setCamera(editorCam);
    showLightWires = data->showLightWires;
}

EditorData* SceneViewWidget::getEditorData()
{
    auto data = new EditorData();
    data->editorCamera = editorCam;
    data->distFromPivot = orbitalCam->distFromPivot;
    data->showLightWires = showLightWires;

    return data;
}

void SceneViewWidget::startPlayingScene()
{
    playScene = true;
    animTime = 0.0f;
}

void SceneViewWidget::stopPlayingScene()
{
    playScene = false;
    animTime = 0.0f;
    scene->updateSceneAnimation(0.0f);
}

iris::ForwardRendererPtr SceneViewWidget::getRenderer() const
{
    return renderer;
}
