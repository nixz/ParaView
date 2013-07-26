/*=========================================================================

   Program: ParaView
   Module:    vtkVRGrabWorldStyle.cxx

   Copyright (c) 2005,2006 Sandia Corporation, Kitware Inc.
   All rights reserved.

   ParaView is a free software; you can redistribute it and/or modify it
   under the terms of the ParaView license version 1.2.

   See License_v1.2.txt for the full ParaView license.
   A copy of this license can be obtained by contacting
   Kitware Inc.
   28 Corporate Drive
   Clifton Park, NY 12065
   USA

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

========================================================================*/
#include "vtkVRGrabWorldStyle.h"

#include "vtkObjectFactory.h"
#include "vtkPVXMLElement.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMProxy.h"
#include "vtkSMRenderViewProxy.h"
#include "vtkVRQueue.h"
#include "vtkNew.h"
#include "vtkTransform.h"
#include "vtkMatrix4x4.h"
#include "vtkCamera.h"

#include "pqView.h"
#include "pqActiveObjects.h"
#include "pqRenderView.h"

#include <sstream>
#include <algorithm>

// ----------------------------------------------------------------------------
vtkStandardNewMacro(vtkVRGrabWorldStyle)

// ----------------------------------------------------------------------------
// Constructor method
vtkVRGrabWorldStyle::vtkVRGrabWorldStyle() :
  Superclass()
{
  this->AddButtonRole("Rotate world");
  this->AddButtonRole("Translate world");
  this->AddButtonRole("Reset world");
  this->EnableTranslate = false;
  this->EnableRotate = false;
  this->IsInitialTransRecorded = false;
  this->IsInitialRotRecorded = false;
  this->CachedTransMatrix->Identity();
  this->CachedRotMatrix->Identity();
}

// ----------------------------------------------------------------------------
// Destructor method
vtkVRGrabWorldStyle::~vtkVRGrabWorldStyle()
{
}

// ----------------------------------------------------------------------------
// PrintSelf() method
void vtkVRGrabWorldStyle::PrintSelf(ostream &os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);

  os << indent << "EnableTranslate: " << this->EnableTranslate << endl;
  os << indent << "EnableRotate: " << this->EnableRotate << endl;
  os << indent << "IsInitialTransRecorded: " << this->IsInitialTransRecorded << endl;
  os << indent << "IsInitialRotRecorded: " << this->IsInitialRotRecorded << endl;
  os << indent << "InverseInitialTransMatrix:" << endl;
  this->InverseInitialTransMatrix->PrintSelf(os, indent.GetNextIndent());
}

// ----------------------------------------------------------------------------
// GetCamera() method
//
// NOTE: in the vtkVRSpaceNavigatorGrabWorldStyle.cxx code, the multiple steps used to
//   get the "rview->getProxy()" value is simply replaced with "this->ControlledProxy".
//   I would like to get an explanation so we know if we can do the same here.
vtkCamera *vtkVRGrabWorldStyle::GetCamera()
{
      vtkCamera		*camera = NULL;
      pqActiveObjects	&activeObjs = pqActiveObjects::instance();

      /* Alert!  The following conditional uses a lazy assignment/evaluation -- the "=" is not a bug (BS: I assume) */
      if (pqView *pqview = activeObjs.activeView()) {
        /* Alert!  The following conditional uses a lazy assignment/evaluation -- the "=" is not a bug (BS: I assume) */
        if (pqRenderView *rview = qobject_cast<pqRenderView*>(pqview)) {
          /* Alert!  The following conditional uses a lazy assignment/evaluation -- the "=" is not a bug (BS: I assume) */
          if (vtkSMRenderViewProxy *rviewPxy = vtkSMRenderViewProxy::SafeDownCast(rview->getProxy())) {
            camera = rviewPxy->GetActiveCamera();
          }
        }
      }
      if (!camera) {
        vtkWarningMacro(<<"Cannot grab active camera.");
      }

      return camera;
}


// ----------------------------------------------------------------------------
// HandleButton() method
void vtkVRGrabWorldStyle::HandleButton(const vtkVREvent& event)
{
  vtkStdString role = this->GetButtonRole(event.name);
  if (role == "Translate world")
    {
    this->EnableTranslate = event.data.button.state;
    }
  else if (role == "Rotate world")
    {
    this->EnableRotate = event.data.button.state;
    }
  else if (event.data.button.state && role == "Reset world")
    {
    this->CachedTransMatrix->Identity();
    this->CachedRotMatrix->Identity();

    /* BS: is there a way to get this property-helper method once at the initialization? */
    vtkSMPropertyHelper(this->ControlledProxy,
                        this->ControlledPropertyName).Set(&this->CachedTransMatrix->Element[0][0], 16);

    vtkSMPropertyHelper(this->ControlledProxy,
                        this->ControlledPropertyName).Set(&this->CachedRotMatrix->Element[0][0], 16);

    this->IsInitialTransRecorded = false;
    this->IsInitialRotRecorded = false;
    }
}

// ----------------------------------------------------------------------------
// HandleTracker() method
void vtkVRGrabWorldStyle::HandleTracker(const vtkVREvent& event)
{
  vtkCamera	*camera = NULL;
  vtkStdString	role = this->GetTrackerRole(event.name);

  if (role == "Tracker")
    {
    if (this->EnableTranslate) {
      /*********************************/
      /*** Handle translation event! ***/
      this->IsInitialRotRecorded = false;	/* disable any current rotation event.   BS: why? */

      /* get the active camera */
      camera = vtkVRGrabWorldStyle::GetCamera();
      if (!camera)
        {
        vtkWarningMacro(<< " HandleTracker: Cannot grab active camera.");
        return;
        }

      // vtkCamera API is misleading...view transform == modelview
      vtkNew<vtkMatrix4x4> rotMatrix;

      /* BS: what is this camera speed factor all about??? */
      double speed = GetSpeedFactor(camera);

      if (!this->IsInitialTransRecorded)
        {
        vtkWarningMacro(<< " camera 'speed' is:" << speed);
        this->InverseInitialTransMatrix->Identity();
        this->InverseInitialTransMatrix->SetElement(0, 3, event.data.tracker.matrix[3] * speed);
        this->InverseInitialTransMatrix->SetElement(1, 3, event.data.tracker.matrix[7] * speed);
        this->InverseInitialTransMatrix->SetElement(2, 3, event.data.tracker.matrix[11] * speed);
        this->InverseInitialTransMatrix->Invert();
        this->IsInitialTransRecorded = true;

        /* This used to be before the IsInitialTransRecorded conditional, and I'm not sure that makes */
        /*   sense.  Why wouldn't this only be done once at the beginning of the grab operation?      */
        rotMatrix->DeepCopy(camera->GetModelViewTransformMatrix());
        rotMatrix->SetElement(0, 3, 0.0);
        rotMatrix->SetElement(1, 3, 0.0);
        rotMatrix->SetElement(2, 3, 0.0);
        rotMatrix->Invert();
        }
      else
        {
        // Get the current tracker matrix, update with initial data
        /* BS: should this be done every time through the loop?  Why isn't this being done just once? */
        vtkNew<vtkMatrix4x4> transformMatrix;
        transformMatrix->Identity();
        vtkNew<vtkMatrix4x4> totalTrackerMatrix;
        totalTrackerMatrix->Identity();
        vtkNew<vtkMatrix4x4> tempMatrix;
        tempMatrix->Identity();

        transformMatrix->SetElement(0, 3, event.data.tracker.matrix[3] * speed);
        transformMatrix->SetElement(1, 3, event.data.tracker.matrix[7] * speed);
        transformMatrix->SetElement(2, 3, event.data.tracker.matrix[11] * speed);
        tempMatrix->DeepCopy(transformMatrix.GetPointer());

        vtkMatrix4x4::Multiply4x4(transformMatrix.GetPointer(),
                                  this->InverseInitialTransMatrix.GetPointer(),
                                  transformMatrix.GetPointer());

        // Compute all transformations via tracker
        vtkMatrix4x4::Multiply4x4(this->CachedTransMatrix.GetPointer(),
                                  transformMatrix.GetPointer(),
                                  totalTrackerMatrix.GetPointer());

        // Store all transformations
        this->CachedTransMatrix->DeepCopy(totalTrackerMatrix.GetPointer());

#if 0 /* NOTE: removing this bit gets rid of the wildly spinning world, but then causes jumping between two matrix states */
        // Apply rotation
        vtkMatrix4x4::Multiply4x4(totalTrackerMatrix.GetPointer(),
                                  rotMatrix.GetPointer(),
                                  totalTrackerMatrix.GetPointer());
#else
/* this almost works, but the rotation and translation are occurring in their own little worlds */
        vtkMatrix4x4::Multiply4x4(totalTrackerMatrix.GetPointer(),
                                  totalTrackerMatrix.GetPointer(),
                                  rotMatrix.GetPointer());
#endif

        transformMatrix->DeepCopy(totalTrackerMatrix.GetPointer());

        // Set the new matrix for the proxy.
        vtkSMPropertyHelper(this->ControlledProxy,
                            this->ControlledPropertyName).Set(&transformMatrix->Element[0][0], 16);
        this->ControlledProxy->UpdateVTKObjects();

        // update the initial matrix to prepare for the next event.
        this->InverseInitialTransMatrix->DeepCopy(tempMatrix.GetPointer());
        this->InverseInitialTransMatrix->Invert();
        }
      }
    else if (this->EnableRotate) {
      /******************************/
      /*** Handle rotation event! ***/
      this->IsInitialTransRecorded = false;	/* disable any current translation event.   BS: why? */

      /* get the active camera */
      camera = vtkVRGrabWorldStyle::GetCamera();
      if (!camera) {
        vtkWarningMacro(<< " HandleTracker: Cannot grab active camera.");
        return;
      }

          vtkNew<vtkMatrix4x4> modelViewTransMatrix;

      if (!this->IsInitialRotRecorded)
        {
        this->InverseInitialRotMatrix->Identity();
        this->InverseInitialRotMatrix->DeepCopy(event.data.tracker.matrix);
        this->InverseInitialRotMatrix->SetElement(0, 3, 0);
        this->InverseInitialRotMatrix->SetElement(1, 3, 0);
        this->InverseInitialRotMatrix->SetElement(2, 3, 0);
        this->InverseInitialRotMatrix->Invert();
        this->IsInitialRotRecorded = true;
        }
      else
        {

  this->CachedTransMatrix->DeepCopy(modelViewTransMatrix.GetPointer());

  // vtkCamera API is misleading...view transform == modelview
  /*  BS: it seems like this should only be done once, when recording the "InverseInitialRotMatrix" matrix */
  modelViewTransMatrix->Identity();
  modelViewTransMatrix->SetElement(0,3, camera->GetModelTransformMatrix()->GetElement(0,3));
  modelViewTransMatrix->SetElement(1,3, camera->GetModelTransformMatrix()->GetElement(1,3));
  modelViewTransMatrix->SetElement(2,3, camera->GetModelTransformMatrix()->GetElement(2,3));

          // Get the current tracker matrix, update with initial data
          /* BS: what about this?  Should it be done only once? */
          vtkNew<vtkMatrix4x4> transformMatrix;
          transformMatrix->Identity();
          vtkNew<vtkMatrix4x4> totalTrackerMatrix;
          totalTrackerMatrix->Identity();
          vtkNew<vtkMatrix4x4> tempMatrix;
          tempMatrix->Identity();

          transformMatrix->DeepCopy(event.data.tracker.matrix);
          transformMatrix->SetElement(0, 3, 0);
          transformMatrix->SetElement(1, 3, 0);
          transformMatrix->SetElement(2, 3, 0);

          tempMatrix->DeepCopy(transformMatrix.GetPointer());

          vtkMatrix4x4::Multiply4x4(transformMatrix.GetPointer(),
                                    this->InverseInitialRotMatrix.GetPointer(),
                                    transformMatrix.GetPointer());

          // Compute all transformations via tracker
          vtkMatrix4x4::Multiply4x4(transformMatrix.GetPointer(),
                                    this->CachedRotMatrix.GetPointer(),
                                    totalTrackerMatrix.GetPointer());

          // Compute new model transform matrix
          vtkMatrix4x4::Multiply4x4(modelViewTransMatrix.GetPointer(),
                                    totalTrackerMatrix.GetPointer(),
                                    transformMatrix.GetPointer());

          // Store all transformations
          this->CachedRotMatrix->DeepCopy(totalTrackerMatrix.GetPointer());

          // Set the new matrix for the proxy.
          vtkSMPropertyHelper(this->ControlledProxy,
                              this->ControlledPropertyName).Set(&transformMatrix->Element[0][0], 16);

          this->ControlledProxy->UpdateVTKObjects();

          // update the initial matrix to prepare for the next event.
          this->InverseInitialRotMatrix->DeepCopy(tempMatrix.GetPointer());
          this->InverseInitialRotMatrix->Invert();
        }
      }
    else
      {
      this->IsInitialTransRecorded = false;
      this->IsInitialRotRecorded = false;
      }
    }
}

// ----------------------------------------------------------------------------
float vtkVRGrabWorldStyle::GetSpeedFactor(vtkCamera *cam)
{
  // Return the distance between the camera and the focal point.
  // BS: and the distance between the camera and the focal point is a speed factor???
  double pos[3];
  double foc[3];
  cam->GetPosition(pos);
  cam->GetFocalPoint(foc);
  pos[0] -= foc[0];
  pos[1] -= foc[1];
  pos[2] -= foc[2];
  pos[0] *= pos[0];
  pos[1] *= pos[1];
  pos[2] *= pos[2];
  return sqrt(pos[0] + pos[1] + pos[2]);
}

// ----------------------------------------------------------------------------
// Update() method
// BS: I'm not sure what Update really does.  Can someone please explain the purpose?
//   Also, why don't all the interactor styles have this method?
