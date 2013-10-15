/*=========================================================================

   Program: ParaView
   Module:    vtkVRGrabTransformStyle.cxx

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
#include "vtkVRGrabTransformStyle.h"

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
vtkStandardNewMacro(vtkVRGrabTransformStyle)

// ----------------------------------------------------------------------------
// Constructor method
vtkVRGrabTransformStyle::vtkVRGrabTransformStyle() :
  Superclass()
{
  this->AddButtonRole("Navigate world");
  this->AddButtonRole("Reset world");
  this->EnableNavigate = false;
  this->IsInitialRecorded = false;
}

// ----------------------------------------------------------------------------
// Destructor method
vtkVRGrabTransformStyle::~vtkVRGrabTransformStyle()
{
}

// ----------------------------------------------------------------------------
// PrintSelf() method
void vtkVRGrabTransformStyle::PrintSelf(ostream &os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);

  os << indent << "EnableNavigate: " << this->EnableNavigate << endl;
  os << indent << "IsInitialRecorded: " << this->IsInitialRecorded << endl;
}

// ----------------------------------------------------------------------------
// GetCamera() method
//
// NOTE: in the vtkVRSpaceNavigatorGrabWorldStyle.cxx code, the multiple steps used to
//   get the "rview->getProxy()" value is simply replaced with "this->ControlledProxy".
//   I would like to get an explanation so we know if we can do the same here.
vtkCamera *vtkVRGrabTransformStyle::GetCamera()
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
void vtkVRGrabTransformStyle::HandleButton(const vtkVREvent& event)
{
  vtkStdString role = this->GetButtonRole(event.name);
  if (role == "Navigate world")
    {
    vtkCamera	*camera = NULL;
    this->EnableNavigate = event.data.button.state;
    if(!this->EnableNavigate)
      {
      this->IsInitialRecorded = false;
      }
    }
  else if (event.data.button.state && role == "Reset world")
    {
    vtkNew<vtkMatrix4x4> transformMatrix;
    /* BS: is there a way to get this property-helper method once at the initialization? */
    vtkSMPropertyHelper(this->ControlledProxy,
                        this->ControlledPropertyName).Set(&transformMatrix.GetPointer()->Element[0][0], 16);
    this->IsInitialRecorded = false;
    }
}

// ----------------------------------------------------------------------------
// HandleTracker() method
void vtkVRGrabTransformStyle::HandleTracker(const vtkVREvent& event)
{
  vtkCamera	*camera = NULL;
  vtkStdString	role = this->GetTrackerRole(event.name);
  if(role!="Tracker") return;
  if(this->EnableNavigate)
    {
      camera = vtkVRGrabTransformStyle::GetCamera();
      if (!camera)
        {
        vtkWarningMacro(<< " HandleTracker: Cannot grab active camera.");
        return;
        }
      double speed = GetSpeedFactor(camera);

    if (!this->IsInitialRecorded)
        {
        // save the ModelView Matrix the Wand Matrix
        this->SavedModelViewMatrix->DeepCopy(camera->GetModelTransformMatrix());
        this->SavedInverseWandMatrix->DeepCopy(event.data.tracker.matrix);

        this->SavedInverseWandMatrix->SetElement(0, 3, event.data.tracker.matrix[3] * speed);
        this->SavedInverseWandMatrix->SetElement(1, 3, event.data.tracker.matrix[7] * speed);
        this->SavedInverseWandMatrix->SetElement(2, 3, event.data.tracker.matrix[11] * speed);
        vtkMatrix4x4::Invert(this->SavedInverseWandMatrix.GetPointer(),
                             this->SavedInverseWandMatrix.GetPointer());
        this->IsInitialRecorded = true;
        vtkWarningMacro(<< "button is pressed");
        }
    else
        {
     // Apply the transfromation and get the new transformation matrix
     vtkNew<vtkMatrix4x4> transformMatrix;
     transformMatrix->DeepCopy(event.data.tracker.matrix);
     transformMatrix->SetElement(0, 3, event.data.tracker.matrix[3] * speed);
     transformMatrix->SetElement(1, 3, event.data.tracker.matrix[7] * speed);
     transformMatrix->SetElement(2, 3, event.data.tracker.matrix[11] * speed);

     vtkMatrix4x4::Multiply4x4(this->SavedInverseWandMatrix.GetPointer(),
                               transformMatrix.GetPointer(),
                               transformMatrix.GetPointer());
     vtkMatrix4x4::Multiply4x4(this->SavedModelViewMatrix.GetPointer(),
                               transformMatrix.GetPointer(),
                               transformMatrix.GetPointer());

      // Set the new matrix for the proxy.
      vtkSMPropertyHelper(this->ControlledProxy,
                          this->ControlledPropertyName).Set(&transformMatrix->Element[0][0], 16);
      this->ControlledProxy->UpdateVTKObjects();
      vtkWarningMacro(<< "button is continuously pressed");
    }
  }
}

// ----------------------------------------------------------------------------
float vtkVRGrabTransformStyle::GetSpeedFactor(vtkCamera *cam)
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
