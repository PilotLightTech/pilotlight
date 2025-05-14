#include "editor.h"

void
pl__camera_update_imgui(plCamera* ptCamera)
{
    static float gfOriginalFOV = 0.0f;
    if(gfOriginalFOV == 0.0f)
        gfOriginalFOV = ptCamera->fFieldOfView;

    if(gptGizmo->active())
        return;


    static const float gfCameraTravelSpeed = 4.0f;
    static const float fCameraRotationSpeed = 0.005f;

    float fCameraTravelSpeed = gfCameraTravelSpeed;

    bool bOwnKeyboard = gptUI->wants_keyboard_capture();
    bool bOwnMouse = gptUI->wants_mouse_capture();

    plIO* ptIO = gptIO->get_io();

    if(!bOwnKeyboard && !bOwnMouse)
    {

        bool bRMB = ImGui::IsMouseDown(ImGuiMouseButton_Right);
        bool bLMB = ImGui::IsMouseDown(ImGuiMouseButton_Left);

        if(ImGui::IsMouseClicked(ImGuiMouseButton_Right, false))
        {
            gfOriginalFOV = ptCamera->fFieldOfView;
        }
        else if(ImGui::IsMouseReleased(ImGuiMouseButton_Right))
        {
            ptCamera->fFieldOfView = gfOriginalFOV;
        }

        if(ImGui::IsKeyDown(ImGuiKey_ModShift))
            fCameraTravelSpeed *= 3.0f;


        // camera space
        
        if(bRMB)
        {
            if(ImGui::IsKeyDown(ImGuiKey_W)) gptCamera->translate(ptCamera,  0.0f,  0.0f,  fCameraTravelSpeed * ptIO->fDeltaTime);
            if(ImGui::IsKeyDown(ImGuiKey_S)) gptCamera->translate(ptCamera,  0.0f,  0.0f, -fCameraTravelSpeed* ptIO->fDeltaTime);
            if(ImGui::IsKeyDown(ImGuiKey_A)) gptCamera->translate(ptCamera, -fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f,  0.0f);
            if(ImGui::IsKeyDown(ImGuiKey_D)) gptCamera->translate(ptCamera,  fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f,  0.0f);

            // world space
            if(ImGui::IsKeyDown(ImGuiKey_Q)) { gptCamera->translate(ptCamera,  0.0f, -fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f); }
            if(ImGui::IsKeyDown(ImGuiKey_E)) { gptCamera->translate(ptCamera,  0.0f,  fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f); }

            if(ImGui::IsKeyDown(ImGuiKey_Z))
            {
                ptCamera->fFieldOfView += 0.25f * (PL_PI / 180.0f);
                ptCamera->fFieldOfView = pl_minf(ptCamera->fFieldOfView, 2.96706f);
            }
            if(ImGui::IsKeyDown(ImGuiKey_C))
            {
                ptCamera->fFieldOfView -= 0.25f * (PL_PI / 180.0f);

                ptCamera->fFieldOfView = pl_maxf(ptCamera->fFieldOfView, 0.03f);
            }
        }

        if(bLMB && ImGui::IsMouseDragging(ImGuiMouseButton_Right, 1.0f))
        {
            const ImVec2 tMouseDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right, 1.0f);
            gptCamera->translate(ptCamera,  tMouseDelta.x * fCameraTravelSpeed * ptIO->fDeltaTime, -tMouseDelta.y * fCameraTravelSpeed * ptIO->fDeltaTime, 0.0f);
            ImGui::ResetMouseDragDelta(ImGuiMouseButton_Right);
            ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
        }

        else if(ImGui::IsMouseDragging(ImGuiMouseButton_Right, 1.0f))
        {
            const ImVec2 tMouseDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right, 1.0f);
            gptCamera->rotate(ptCamera,  -tMouseDelta.y * fCameraRotationSpeed,  -tMouseDelta.x * fCameraRotationSpeed);
            ImGui::ResetMouseDragDelta(ImGuiMouseButton_Right);
        }

        else if(bLMB)
        {
            const ImVec2 tMouseDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 1.0f);
            gptCamera->rotate(ptCamera,  0.0f,  -tMouseDelta.x * fCameraRotationSpeed);
            ptCamera->tPos.x += -tMouseDelta.y * fCameraTravelSpeed * ptIO->fDeltaTime * sinf(ptCamera->fYaw);
            ptCamera->tPos.z += -tMouseDelta.y * fCameraTravelSpeed * ptIO->fDeltaTime * cosf(ptCamera->fYaw);
            ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
        }
    }

    gptCamera->update(ptCamera);
}