#include "pch.h"
#include "Editor/Public/Editor.h"
#include "Editor/Public/Camera.h"
#include "Editor/Public/Axis.h"
#include "Render/Renderer/Public/Renderer.h"
#include "Manager/UI/Public/UIManager.h"
#include "Manager/Input/Public/InputManager.h"
#include "Manager/Config/Public/ConfigManager.h"
#include "Manager/Time/Public/TimeManager.h"
#include "Component/Public/PrimitiveComponent.h"
#include "Level/Public/Level.h"
#include "Global/Quaternion.h"
#include "Utility/Public/ScopeCycleCounter.h"
#include "Render/UI/Overlay/Public/StatOverlay.h"
#include "Component/Public/DecalSpotLightComponent.h"
#include "Component/Public/PointLightComponent.h"
#include "Physics/Public/BoundingSphere.h"
#include "Component/Public/DirectionalLightComponent.h"
#include "Component/Public/SpotLightComponent.h"
#include "Component/Public/EditorIconComponent.h"
#include "Component/Public/BillBoardComponent.h"
#include "Manager/UI/Public/ViewportManager.h"
#include "Render/UI/Overlay/Public/D2DOverlayManager.h"
#include "Render/ui/Viewport/Public/ViewportClient.h"
#include "Render/UI/Viewport/Public/Viewport.h"

IMPLEMENT_CLASS(UEditor, UObject)

UEditor::UEditor()
{
	int32 ActiveIndex = UViewportManager::GetInstance().GetActiveIndex();
	ActiveViewportIndex = ActiveIndex;
	Camera = UViewportManager::GetInstance().GetClients()[ActiveIndex]->GetCamera();
}

UEditor::~UEditor()
{
	// 레거시 코드 제거: ViewportManager가 뷰포트 관리
}

void UEditor::Update()
{
	UViewportManager& ViewportManager = UViewportManager::GetInstance();
	const UInputManager& Input = UInputManager::GetInstance();

	// 활성 카메라 업데이트
	int32 ActiveIndex = ViewportManager.GetActiveIndex();
	if (ActiveViewportIndex != ActiveIndex)
	{
		ActiveViewportIndex = ActiveIndex;
		Camera = ViewportManager.GetClients()[ActiveIndex]->GetCamera();
	}

	// KTLWeek07: 각 뷰포트에서 마우스 우클릭 시 해당 카메라만 입력 활성화
	int32 HoveredViewportIndex = ViewportManager.GetMouseHoveredViewportIndex();
	bool bIsRightMouseDown = Input.IsKeyDown(EKeyInput::MouseRight);

	// 드래그 시작 시 뷰포트 고정, 드래그 종료 시 해제
	if (bIsRightMouseDown && !bWasRightMouseDown)
	{
		// 우클릭 시작: 현재 호버된 뷰포트를 잠금
		LockedViewportIndexForDrag = HoveredViewportIndex;
	}
	else if (!bIsRightMouseDown && bWasRightMouseDown)
	{
		// 우클릭 종료: 잠금 해제
		LockedViewportIndexForDrag = -1;
	}
	bWasRightMouseDown = bIsRightMouseDown;

	// 드래그 중이면 잠긴 뷰포트 사용, 아니면 호버된 뷰포트 사용
	int32 ActiveViewportIndexForInput = (LockedViewportIndexForDrag >= 0) ? LockedViewportIndexForDrag : HoveredViewportIndex;

	if (ViewportManager.GetViewportLayout() == EViewportLayout::Quad)
	{
		// Quad 모드: 마우스 우클릭 중이고 해당 뷰포트 위에 있을 때만 그 카메라 입력 활성화
		FViewportClient* ActiveOrthoClient = nullptr;

		for (int32 i = 0; i < 4; ++i)
		{
			if (ViewportManager.GetClients()[i] && ViewportManager.GetClients()[i]->GetCamera())
			{
				UCamera* Cam = ViewportManager.GetClients()[i]->GetCamera();
				// 마우스 우클릭 중이고 해당 뷰포트가 활성화된 뷰포트면 카메라 입력 활성화
				bool bEnableInput = (ActiveViewportIndexForInput == i && bIsRightMouseDown);
				Cam->SetInputEnabled(bEnableInput);

				// 오쏘 뷰가 활성화되었고 이동이 있었다면 기록
				if (bEnableInput && ViewportManager.GetClients()[i]->IsOrtho())
				{
					ActiveOrthoClient = ViewportManager.GetClients()[i];
				}
			}
		}

		// 오쏘 뷰 드래그 시 모든 오쏘 뷰를 공유 중심점 기준으로 업데이트
		if (ActiveOrthoClient && bIsRightMouseDown)
		{
			UCamera* ActiveOrthoCam = ActiveOrthoClient->GetCamera();
			if (ActiveOrthoCam)
			{
				// Camera::UpdateInput에서 이미 RelativeLocation이 업데이트됨
				// 공유 중심점 업데이트
				FVector CurrentLocation = ActiveOrthoCam->GetLocation();

				// ViewType에 따라 InitialOffsets 인덱스 결정
				int32 OrthoIdx = -1;
				switch (ActiveOrthoClient->GetViewType())
				{
				case EViewType::OrthoTop: OrthoIdx = 0; break;
				case EViewType::OrthoBottom: OrthoIdx = 1; break;
				case EViewType::OrthoLeft: OrthoIdx = 2; break;
				case EViewType::OrthoRight: OrthoIdx = 3; break;
				case EViewType::OrthoFront: OrthoIdx = 4; break;
				case EViewType::OrthoBack: OrthoIdx = 5; break;
				}

				if (OrthoIdx >= 0 && OrthoIdx < ViewportManager.GetInitialOffsets().size())
				{
					// 공유 중심점 = 현재 위치 - 초기 오프셋
					ViewportManager.SetOrthoGraphicCameraPoint(CurrentLocation - ViewportManager.GetInitialOffsets()[OrthoIdx]);

					// 모든 오쏘 뷰를 공유 중심점 기준으로 업데이트
					for (int32 i = 0; i < 4; ++i)
					{
						if (ViewportManager.GetClients()[i] && ViewportManager.GetClients()[i]->IsOrtho())
						{
							FViewportClient* Client = ViewportManager.GetClients()[i];
							int32 ClientOrthoIdx = -1;
							switch (Client->GetViewType())
							{
							case EViewType::OrthoTop: ClientOrthoIdx = 0; break;
							case EViewType::OrthoBottom: ClientOrthoIdx = 1; break;
							case EViewType::OrthoLeft: ClientOrthoIdx = 2; break;
							case EViewType::OrthoRight: ClientOrthoIdx = 3; break;
							case EViewType::OrthoFront: ClientOrthoIdx = 4; break;
							case EViewType::OrthoBack: ClientOrthoIdx = 5; break;
							}

							if (ClientOrthoIdx >= 0 && ClientOrthoIdx < ViewportManager.GetInitialOffsets().size() && Client->GetCamera())
							{
								FVector NewLocation = ViewportManager.GetOrthoGraphicCameraPoint() + ViewportManager.GetInitialOffsets()[ClientOrthoIdx];
								Client->GetCamera()->SetLocation(NewLocation);
							}
						}
					}
				}
			}
		}
	}
	else
	{
		// 싱글 모드: 뷰포트 위에서 마우스 우클릭 시 카메라 입력 활성화
		if (Camera)
		{
			// Single 모드에서는 ActiveViewportIndexForInput이 유효한 뷰포트면 입력 활성화
			bool bEnableInput = (ActiveViewportIndexForInput >= 0 && bIsRightMouseDown);
			Camera->SetInputEnabled(bEnableInput);
		}
	}

	UpdateBatchLines();
	BatchLines.UpdateVertexBuffer();

	UpdateCameraAnimation();

	// Pilot Mode 키 바인딩 처리
	UInputManager& InputManager = UInputManager::GetInstance();

	// Ctrl + Shift + P: Pilot Mode 진입
	if (InputManager.IsKeyPressed(EKeyInput::P) &&
		InputManager.IsKeyDown(EKeyInput::Ctrl) &&
		InputManager.IsKeyDown(EKeyInput::Shift))
	{
		if (!bIsPilotMode)
		{
			TogglePilotMode();
		}
	}

	// Alt + G: Pilot Mode 해제
	if (InputManager.IsKeyPressed(EKeyInput::G) &&
		InputManager.IsKeyDown(EKeyInput::Alt))
	{
		if (bIsPilotMode)
		{
			ExitPilotMode();
		}
	}

	// Pilot Mode 업데이트
	if (bIsPilotMode)
	{
		UpdatePilotMode();
	}

	ProcessMouseInput();
}

void UEditor::Collect2DRender(UCamera* InCamera, const D3D11_VIEWPORT& InViewport)
{
	// D2D 드로잉 정보 수집 시작
	FD2DOverlayManager& OverlayManager = FD2DOverlayManager::GetInstance();
	OverlayManager.BeginCollect(InCamera, InViewport);

	// FAxis 렌더링 명령 수집
	FAxis::CollectDrawCommands(OverlayManager, InCamera, InViewport);

	// Gizmo 회전 각도 오버레이 수집
	Gizmo.CollectRotationAngleOverlay(OverlayManager, InCamera, InViewport);

	// StatOverlay 렌더링 명령 수집
	UStatOverlay::GetInstance().Render();
}

void UEditor::RenderEditorGeometry()
{
	// 3D 지오메트리 렌더링 (Grid, AABB, Light Lines, Octree 등)
	BatchLines.Render();
}

void UEditor::RenderGizmo(UCamera* InCamera, const D3D11_VIEWPORT& InViewport)
{
	Gizmo.RenderGizmo(InCamera, InViewport);

	// 모든 DirectionalLight의 빛 방향 기즈모 렌더링 (선택 여부 무관)
	UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
	if (EditorWorld && EditorWorld->GetLevel())
	{
		ULevel* CurrentLevel = EditorWorld->GetLevel();
		const TArray<ULightComponent*>& LightComponents = CurrentLevel->GetLightComponents();
		for (ULightComponent* LightComp : LightComponents)
		{
			// Pilot Mode: 현재 조종 중인 Actor의 컴포넌트는 화살표 렌더링 스킵
			if (bIsPilotMode && PilotedActor && LightComp->GetOwner() == PilotedActor)
			{
				continue;
			}

			if (UDirectionalLightComponent* DirLight = Cast<UDirectionalLightComponent>(LightComp))
			{
				DirLight->RenderLightDirectionGizmo(InCamera, InViewport);
			}
			if (USpotLightComponent* SpotLight = Cast<USpotLightComponent>(LightComp))
			{
				SpotLight->RenderLightDirectionGizmo(InCamera, InViewport);
			}
		}
	}
}

void UEditor::RenderGizmoForHitProxy(UCamera* InCamera, const D3D11_VIEWPORT& InViewport)
{
	if (Gizmo.HasComponent())
	{
		Gizmo.RenderForHitProxy(InCamera, InViewport);
	}
}

void UEditor::UpdateBatchLines()
{
	UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
	if (!EditorWorld || !EditorWorld->GetLevel()) { return; }

	uint64 ShowFlags = EditorWorld->GetLevel()->GetShowFlags();

	if (ShowFlags & EEngineShowFlags::SF_Octree)
	{
		BatchLines.UpdateOctreeVertices(EditorWorld->GetLevel()->GetStaticOctree());
	}
	else
	{
		// If we are not showing the octree, clear the lines, so they don't persist
		BatchLines.ClearOctreeLines();
	}

	if (UActorComponent* Component = GetSelectedComponent())
	{
		if (UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(Component))
		{
			if (ShowFlags & EEngineShowFlags::SF_Bounds)
			{
				if (PrimitiveComponent->GetBoundingBox()->GetType() == EBoundingVolumeType::AABB)
				{
					FVector WorldMin, WorldMax; PrimitiveComponent->GetWorldAABB(WorldMin, WorldMax);
					FAABB AABB(WorldMin, WorldMax);
					BatchLines.UpdateBoundingBoxVertices(&AABB);
				}
				else 
				{ 
					BatchLines.UpdateBoundingBoxVertices(PrimitiveComponent->GetBoundingBox()); 

					// 만약 선택된 타입이 decalspotlightcomponent라면
					if (Component->IsA(UDecalSpotLightComponent::StaticClass()))
					{
						BatchLines.UpdateDecalSpotLightVertices(Cast<UDecalSpotLightComponent>(Component));
					}
				}
				return; 
			}
		}
		if (ULightComponent* LightComponent = Cast<ULightComponent>(Component))
		{
			if (UPointLightComponent* PointLightComponent = Cast<UPointLightComponent>(Component))
			{
				if (USpotLightComponent* SpotLightComponent = Cast<USpotLightComponent>(Component))
				{
					const FVector Center = SpotLightComponent->GetWorldLocation();
					const float Radius = SpotLightComponent->GetAttenuationRadius();
					const float OuterRadian = SpotLightComponent->GetOuterConeAngle();
					const float InnerRadian = SpotLightComponent->GetInnerConeAngle();
					FQuaternion Rotation = SpotLightComponent->GetWorldRotationAsQuaternion();
					BatchLines.UpdateConeVertices(Center, Radius, OuterRadian, InnerRadian, Rotation);
					return;
				}
				const FVector Center = PointLightComponent->GetWorldLocation();
				const float Radius = PointLightComponent->GetAttenuationRadius();

				FBoundingSphere PointSphere(Center, Radius);
				BatchLines.UpdateBoundingBoxVertices(&PointSphere);
				return;
			}
		}
	}

	BatchLines.DisableRenderBoundingBox();
}


void UEditor::ProcessMouseInput()
{
	// KTLWeek07: 활성 카메라 사용 (ViewportManager에서 관리)
	UCamera* CurrentCamera = Camera; // 생성자에서 설정된 활성 카메라
	if (!CurrentCamera)
	{
		return;
	}

	const UInputManager& InputManager = UInputManager::GetInstance();
	const FVector& MousePos = InputManager.GetMousePosition();

	// KTLWeek07: 활성 뷰포트의 정보 가져오기
	auto& ViewportManager = UViewportManager::GetInstance();
	int32 ActiveViewportIndex = ViewportManager.GetActiveIndex();
	FViewport* ActiveViewport = ViewportManager.GetViewports()[ActiveViewportIndex];
	if (!ActiveViewport) { return; }

	// PIE 뷰포트에서는 에디터 입력(오브젝트 선택, 기즈모 조작) 처리 안 함
	if (GEditor->IsPIESessionActive() && ActiveViewportIndex == ViewportManager.GetPIEActiveViewportIndex())
	{
		return;
	}

	const D3D11_VIEWPORT& ViewportInfo = ActiveViewport->GetRenderRect();

	AActor* ActorPicked = GetSelectedActor();
	if (ActorPicked)
	{
		// 피킹 전 현재 카메라에 맞는 기즈모 스케일 업데이트
		Gizmo.UpdateScale(CurrentCamera, ViewportInfo);
	}

	const float NdcX = ((MousePos.X - ViewportInfo.TopLeftX) / ViewportInfo.Width) * 2.0f - 1.0f;
	const float NdcY = -(((MousePos.Y - ViewportInfo.TopLeftY) / ViewportInfo.Height) * 2.0f - 1.0f);

	FRay WorldRay = CurrentCamera->ConvertToWorldRay(NdcX, NdcY);

	static EGizmoDirection PreviousGizmoDirection = EGizmoDirection::None;
	FVector CollisionPoint;
	float ActorDistance = -1;

	// 기즈모 World / Local 모드 전환
	if (InputManager.IsKeyDown(EKeyInput::Ctrl) && InputManager.IsKeyPressed(EKeyInput::Backtick))
	{
		Gizmo.IsWorldMode() ? Gizmo.SetLocal() : Gizmo.SetWorld();
	}
	if (InputManager.IsKeyPressed(EKeyInput::Space))
	{
		Gizmo.ChangeGizmoMode();
	}

	// W/E/R 키로 기즈모 모드 직접 전환 (우클릭 중이 아닐 때만)
	bool bIsRightMouseDown = InputManager.IsKeyDown(EKeyInput::MouseRight);
	if (!bIsRightMouseDown)
	{
		if (InputManager.IsKeyPressed(EKeyInput::W))
		{
			Gizmo.SetGizmoMode(EGizmoMode::Translate);
		}
		if (InputManager.IsKeyPressed(EKeyInput::E))
		{
			Gizmo.SetGizmoMode(EGizmoMode::Rotate);
		}
		if (InputManager.IsKeyPressed(EKeyInput::R))
		{
			Gizmo.SetGizmoMode(EGizmoMode::Scale);
		}
	}
	if (InputManager.IsKeyReleased(EKeyInput::MouseLeft))
	{
		Gizmo.EndDrag();

		// 복사 모드 종료
		if (bIsInCopyMode)
		{
			bIsInCopyMode = false;
			CopiedActor = nullptr;
			CopiedComponent = nullptr;
		}
	}

	// 스플리터 드래그 여부 체크
	const bool bSplitterDragging = ViewportManager.IsAnySplitterDragging();

	if (Gizmo.IsDragging() && IsValid<USceneComponent>(Gizmo.GetSelectedComponent()))
	{
		switch (Gizmo.GetGizmoMode())
		{
		case EGizmoMode::Translate:
		{
			FVector GizmoDragLocation = GetGizmoDragLocation(CurrentCamera, WorldRay);
			Gizmo.SetLocation(GizmoDragLocation);
			break;
		}
		case EGizmoMode::Rotate:
		{
			FQuaternion GizmoDragRotation = GetGizmoDragRotation(CurrentCamera, WorldRay);
			Gizmo.SetComponentRotation(GizmoDragRotation);
			break;
		}
		case EGizmoMode::Scale:
		{
			FVector GizmoDragScale = GetGizmoDragScale(CurrentCamera, WorldRay);
			Gizmo.SetComponentScale(GizmoDragScale);
		}
		}
	}
	else if (!ImGui::GetIO().WantCaptureMouse && !bSplitterDragging)
	{
		// 스플리터 드래그 중이 아니고, ImGui가 마우스를 캡처하지 않았을 때만 처리
		if (GetSelectedActor() && Gizmo.HasComponent())
		{
			// 기즈모 호버링 (Ray Based)
			ObjectPicker.PickGizmo(CurrentCamera, WorldRay, Gizmo, CollisionPoint);
		}
		else
		{
			Gizmo.SetGizmoDirection(EGizmoDirection::None);
		}

		// Esc 키로 선택 해제
		if (InputManager.IsKeyPressed(EKeyInput::Esc))
		{
			SelectActor(nullptr);
			SelectComponent(nullptr);
			bIsActorSelected = true;
		}

		// Delete 키로 컴포넌트 제거
		if (InputManager.IsKeyPressed(EKeyInput::Delete))
		{
			AActor* CurrentSelectedActor = GetSelectedActor();
			UActorComponent* CurrentSelectedComponent = GetSelectedComponent();

			if (CurrentSelectedActor && CurrentSelectedComponent)
			{
				// RootComponent는 삭제할 수 없음
				if (CurrentSelectedComponent == CurrentSelectedActor->GetRootComponent())
				{
					UE_LOG_WARNING("Editor: RootComponent는 삭제할 수 없습니다.");
				}
				else
				{
					// 컴포넌트 삭제
					FString ComponentName = CurrentSelectedComponent->GetName().ToString();
					if (CurrentSelectedActor->RemoveComponent(CurrentSelectedComponent, false))
					{
						UE_LOG_SUCCESS("Editor: 컴포넌트 '%s'를 삭제했습니다.", ComponentName.c_str());
						SelectComponent(nullptr);
					}
					else
					{
						UE_LOG_ERROR("Editor: 컴포넌트 '%s' 삭제에 실패했습니다.", ComponentName.c_str());
					}
				}
			}
		}

		// 더블 클릭이 우선 (단일 클릭보다 먼저 체크)
		bool bIsDoubleClick = InputManager.IsMouseDoubleClicked(EKeyInput::MouseLeft);
		bool bIsSingleClick = !bIsDoubleClick && InputManager.IsKeyPressed(EKeyInput::MouseLeft);

		if (bIsDoubleClick || bIsSingleClick)
		{
			// 뷰포트 클릭 시 LastClickedViewportIndex 업데이트 (PIE 시작 시 사용)
			ViewportManager.SetLastClickedViewportIndex(ActiveViewportIndex);

			// HitProxy를 통한 컴포넌트 피킹
			UPrimitiveComponent* PrimitiveCollided = nullptr;
			const int32 MouseX = static_cast<int32>(MousePos.X - ViewportInfo.TopLeftX);
			const int32 MouseY = static_cast<int32>(MousePos.Y - ViewportInfo.TopLeftY);

			TStatId StatId("Picking");
			FScopeCycleCounter PickCounter(StatId);
			PrimitiveCollided = ObjectPicker.PickPrimitiveFromHitProxy(CurrentCamera, MouseX, MouseY);
			ActorPicked = PrimitiveCollided ? PrimitiveCollided->GetOwner() : nullptr;
			float ElapsedMs = static_cast<float>(PickCounter.Finish());
			UStatOverlay::GetInstance().RecordPickingStats(ElapsedMs);

			// 피킹 결과에 따라 Actor와 Component 선택
			if (Gizmo.GetGizmoDirection() == EGizmoDirection::None)
			{
				if (ActorPicked && PrimitiveCollided)
				{
					UActorComponent* ComponentToSelect = PrimitiveCollided;

					// Visualization 컴포넌트가 피킹된 경우, 부모 컴포넌트를 선택
					if (PrimitiveCollided->IsVisualizationComponent())
					{
						if (USceneComponent* ScenePrim = Cast<USceneComponent>(PrimitiveCollided))
						{
							if (USceneComponent* Parent = ScenePrim->GetAttachParent())
							{
								ComponentToSelect = Parent;
							}
						}
					}

					if (bIsDoubleClick)
					{
						// 더블클릭: Component 피킹 모드로 진입
						SelectActorAndComponent(ActorPicked, ComponentToSelect);
						bIsActorSelected = false;
					}
					else // bIsSingleClick
					{
						AActor* CurrentSelectedActor = GetSelectedActor();

						if (!CurrentSelectedActor)
						{
							// 선택 없음 상태: Actor 선택
							SelectActor(ActorPicked);
							bIsActorSelected = true;
						}
						else if (bIsActorSelected)
						{
							// Actor 선택 상태에서 단일 클릭
							if (CurrentSelectedActor == ActorPicked)
							{
								// 같은 Actor: Actor 선택 유지
								SelectActor(ActorPicked);
								bIsActorSelected = true;
							}
							else
							{
								// 다른 Actor: 새로운 Actor 선택
								SelectActor(ActorPicked);
								bIsActorSelected = true;
							}
						}
						else
						{
							// Component 피킹 모드에서 단일 클릭
							if (CurrentSelectedActor == ActorPicked)
							{
								// 같은 Actor 내 컴포넌트: Component 전환
								SelectActorAndComponent(ActorPicked, ComponentToSelect);
								bIsActorSelected = false;
							}
							else
							{
								// 다른 Actor: Component 피킹 모드 해제 -> Actor 선택
								SelectActor(ActorPicked);
								bIsActorSelected = true;
							}
						}
					}
				}
				else
				{
					// 빈 공간 클릭: 선택 해제
					SelectActor(nullptr);
					bIsActorSelected = true;
				}
			}
		}

		if (Gizmo.GetGizmoDirection() == EGizmoDirection::None)
		{
			if (PreviousGizmoDirection != EGizmoDirection::None)
			{
				Gizmo.OnMouseRelease(PreviousGizmoDirection);
			}
		}
		else
		{
			PreviousGizmoDirection = Gizmo.GetGizmoDirection();
			if (InputManager.IsKeyPressed(EKeyInput::MouseLeft))
			{
				// Alt + 드래그: 객체 복사 (Scale 모드에서는 비활성화)
				bool bAltPressed = InputManager.IsKeyDown(EKeyInput::Alt);
				bool bIsScaleMode = (Gizmo.GetGizmoMode() == EGizmoMode::Scale);

				if (bAltPressed && GetSelectedActor() && GetSelectedComponent() && !bIsScaleMode)
				{
					// 실제로 Actor 선택인지 Component 선택인지 확인
					// RootComponent가 선택된 경우 = Actor 선택
					bool bIsActorSelection = (GetSelectedComponent() == GetSelectedActor()->GetRootComponent());

					if (bIsActorSelection)
					{
						// Actor 복사 (전체)
						AActor* NewActor = DuplicateActor(GetSelectedActor());
						if (NewActor)
						{
							SelectActor(NewActor);
							CopiedActor = NewActor;
							bIsInCopyMode = true;
						}
					}
					else
					{
						// Component 복사 (같은 Actor 내)
						UActorComponent* NewComponent = DuplicateComponent(GetSelectedComponent(), GetSelectedActor());
						if (NewComponent)
						{
							SelectActorAndComponent(GetSelectedActor(), NewComponent);
							CopiedComponent = NewComponent;
							bIsInCopyMode = true;
						}
					}
				}

				Gizmo.OnMouseDragStart(CollisionPoint);
			}
			else
			{
				Gizmo.OnMouseHovering();
			}
		}
	}

	if (InputManager.IsKeyPressed(EKeyInput::F))
	{
		FocusOnSelectedActor();
	}
}

FVector UEditor::GetGizmoDragLocation(UCamera* InActiveCamera, FRay& WorldRay)
{
	FVector MouseWorld;
	FVector PlaneOrigin{ Gizmo.GetGizmoLocation() };

	// Center 구체 드래그 처리
	// UE 기준 카메라 NDC 평면에 평행하게 이동
	if (Gizmo.GetGizmoDirection() == EGizmoDirection::Center)
	{
		// 카메라의 Forward 방향을 평면 법선로 사용
		FVector PlaneNormal = InActiveCamera->GetForward();

		// 드래그 시작 지점의 마우스 위치를 평면 원점으로 사용
		FVector FixedPlaneOrigin = Gizmo.GetDragStartMouseLocation();

		// 레이와 카메라 평면 교차점 계산
		if (ObjectPicker.IsRayCollideWithPlane(WorldRay, FixedPlaneOrigin, PlaneNormal, MouseWorld))
		{
			// 드래그 시작점으로부터 이동 거리 계산
			FVector MouseDelta = MouseWorld - Gizmo.GetDragStartMouseLocation();
			return Gizmo.GetDragStartActorLocation() + MouseDelta;
		}
		return Gizmo.GetGizmoLocation();
	}

	// 평면 드래그 처리
	if (Gizmo.IsPlaneDirection())
	{
		// 평면 법선 벡터
		FVector PlaneNormal = Gizmo.GetPlaneNormal();

		if (!Gizmo.IsWorldMode())
		{
			FQuaternion q = Gizmo.GetTargetComponent()->GetWorldRotationAsQuaternion();
			PlaneNormal = q.RotateVector(PlaneNormal);
		}

		// 레이와 평면 교차점 계산
		if (ObjectPicker.IsRayCollideWithPlane(WorldRay, PlaneOrigin, PlaneNormal, MouseWorld))
		{
			// 드래그 시작점으로부터 이동 거리 계산
			FVector MouseDelta = MouseWorld - Gizmo.GetDragStartMouseLocation();
			return Gizmo.GetDragStartActorLocation() + MouseDelta;
		}
		return Gizmo.GetGizmoLocation();
	}

	// 축 드래그 처리
	FVector GizmoAxis = Gizmo.GetGizmoAxis();

	if (!Gizmo.IsWorldMode())
	{
		FQuaternion q = Gizmo.GetTargetComponent()->GetWorldRotationAsQuaternion();
		GizmoAxis = q.RotateVector(GizmoAxis);
	}

	// 카메라 방향 벡터를 사용하여 안정적인 평면 계산
	const FVector CamRight = InActiveCamera->GetRight();
	const FVector CamUp = InActiveCamera->GetUp();

	// GizmoAxis에 가장 수직인 카메라 벡터 선택
	FVector PlaneVector;
	const float DotRight = abs(GizmoAxis.Dot(CamRight));
	const float DotUp = abs(GizmoAxis.Dot(CamUp));

	if (DotRight < DotUp)
	{
		PlaneVector = CamRight;
	}
	else
	{
		PlaneVector = CamUp;
	}

	FVector PlaneNormal = GizmoAxis.Cross(PlaneVector);
	PlaneNormal.Normalize();

	// PlaneNormal이 카메라를 향하도록 보장 (드래그 방향 일관성)
	const FVector CameraLocation = InActiveCamera->GetLocation();
	const FVector ToCamera = (CameraLocation - PlaneOrigin).GetNormalized();
	if (PlaneNormal.Dot(ToCamera) < 0.0f)
	{
		PlaneNormal = -PlaneNormal;
	}

	if (ObjectPicker.IsRayCollideWithPlane(WorldRay, PlaneOrigin, PlaneNormal, MouseWorld))
	{
		FVector MouseDistance = MouseWorld - Gizmo.GetDragStartMouseLocation();
		return Gizmo.GetDragStartActorLocation() + GizmoAxis * MouseDistance.Dot(GizmoAxis);
	}
	return Gizmo.GetGizmoLocation();
}

FQuaternion UEditor::GetGizmoDragRotation(UCamera* InActiveCamera, FRay& WorldRay)
{
	const FVector GizmoLocation = Gizmo.GetGizmoLocation();
	const FVector LocalGizmoAxis = Gizmo.GetGizmoAxis();
	const FQuaternion StartRotQuat = Gizmo.GetDragStartActorRotationQuat();

	// 월드 공간 회전축
	FVector WorldRotationAxis = LocalGizmoAxis;
	if (!Gizmo.IsWorldMode())
	{
		WorldRotationAxis = StartRotQuat.RotateVector(LocalGizmoAxis);
	}

	// 스크린 공간 회전 계산
	// 활성 뷰포트 정보 가져오기
	const FRect& ViewportRect = UViewportManager::GetInstance().GetActiveViewportRect();
	const float ViewportWidth = static_cast<float>(ViewportRect.Width);
	const float ViewportHeight = static_cast<float>(ViewportRect.Height);
	const float ViewportLeft = static_cast<float>(ViewportRect.Left);
	const float ViewportTop = static_cast<float>(ViewportRect.Top);

	// 기즈모 중심을 스크린 공간으로 투영
	const FCameraConstants& CamConst = InActiveCamera->GetFViewProjConstants();
	const FMatrix ViewProj = CamConst.View * CamConst.Projection;
	FVector4 GizmoScreenPos4 = FVector4(GizmoLocation, 1.0f) * ViewProj;

	if (GizmoScreenPos4.W > 0.0f)
	{
		// NDC로 변환
		GizmoScreenPos4 *= (1.0f / GizmoScreenPos4.W);

		// NDC → 뷰포트 로컬 좌표
		const FVector2 GizmoScreenPos(
			(GizmoScreenPos4.X * 0.5f + 0.5f) * ViewportWidth,
			((-GizmoScreenPos4.Y) * 0.5f + 0.5f) * ViewportHeight
		);

		// 현재 마우스 스크린 좌표
		POINT MousePos;
		GetCursorPos(&MousePos);
		ScreenToClient(GetActiveWindow(), &MousePos);

		const FVector2 CurrentScreenPos(
			static_cast<float>(MousePos.x) - ViewportLeft,
			static_cast<float>(MousePos.y) - ViewportTop
		);

		// UE5 표준: 드래그 시작 지점에서 기즈모로의 방향 (Origin = DragStartPos in UE)
		const FVector2 DragStartScreenPos = Gizmo.GetDragStartScreenPos();
		const FVector2 DirectionToMousePos = (DragStartScreenPos - GizmoScreenPos).GetNormalized();

		// Tangent 방향: DirectionToMousePos에 수직 (시계방향 회전)
		FVector2 TangentDir = FVector2(-DirectionToMousePos.Y, DirectionToMousePos.X);

		// 각 축마다 TangentDir 방향 조정
		if (Gizmo.GetGizmoDirection() == EGizmoDirection::Right)
		{
			TangentDir = -TangentDir;
		}

		// 스크린 공간 드래그 벡터 (UE5 표준: Y축 반전)
		const FVector2 PrevScreenPos = Gizmo.GetPreviousScreenPos();
		const FVector2 DragDelta = CurrentScreenPos - PrevScreenPos;
		const FVector2 DragDir = FVector2(DragDelta.X, -DragDelta.Y); // Y축 반전

		// 마우스가 실제로 움직였는지 체크
		const float DragDistSq = DragDir.LengthSquared();
		constexpr float MinDragDistSq = 0.1f * 0.1f;

		if (DragDistSq > MinDragDistSq)
		{
			// UE5 표준: Dot Product로 회전 각도 계산
			float PixelDelta = TangentDir.Dot(DragDir);

			// 픽셀을 각도(라디안)로 변환 (언리얼에선 1픽셀 = 1도 사용, 어차피 감도 조절용 매직 넘버라 그대로 차용)
			constexpr float PixelsToDegrees = 1.0f;
			float DeltaAngleDegrees = PixelDelta * PixelsToDegrees;
			float DeltaAngle = FVector::GetDegreeToRadian(DeltaAngleDegrees);

			// 누적 각도 업데이트
			float NewAngle = Gizmo.GetCurrentRotationAngle() + DeltaAngle;

			// 360deg Clamp
			constexpr float TwoPi = 2.0f * PI;
			if (NewAngle > TwoPi)
			{
				NewAngle = fmodf(NewAngle, TwoPi);
			}
			else if (NewAngle < -TwoPi)
			{
				NewAngle = fmodf(NewAngle, -TwoPi);
			}

			Gizmo.SetCurrentRotationAngle(NewAngle);
		}

		// 현재 스크린 좌표 저장
		Gizmo.SetPreviousScreenPos(CurrentScreenPos);

		// 최종 회전 Quaternion 계산 (스냅 적용)
		float FinalAngle = Gizmo.GetCurrentRotationAngle();
		if (UViewportManager::GetInstance().IsRotationSnapEnabled())
		{
			const float SnapAngleDegrees = UViewportManager::GetInstance().GetRotationSnapAngle();
			const float SnapAngleRadians = FVector::GetDegreeToRadian(SnapAngleDegrees);
			FinalAngle = std::round(Gizmo.GetCurrentRotationAngle() / SnapAngleRadians) * SnapAngleRadians;
		}

		// 기즈모 드래그 방향과 회전 방향을 일치시키기 위해 부호 반전
		const FQuaternion DeltaRotQuat = FQuaternion::FromAxisAngle(LocalGizmoAxis, FinalAngle);
		if (Gizmo.IsWorldMode())
		{
			return DeltaRotQuat * StartRotQuat;
		}
		else
		{
			return StartRotQuat * DeltaRotQuat;
		}
	}

	return Gizmo.GetComponentRotation();
}

FVector UEditor::GetGizmoDragScale(UCamera* InActiveCamera, FRay& WorldRay)
{
	FVector MouseWorld;
	FVector PlaneOrigin = Gizmo.GetGizmoLocation();
	FQuaternion Quat = Gizmo.GetTargetComponent()->GetWorldRotationAsQuaternion();
	const FVector CameraLocation = InActiveCamera->GetLocation();

	// Center 구체 드래그 처리 (균일 스케일, 모든 축 동일하게)
	if (Gizmo.GetGizmoDirection() == EGizmoDirection::Center)
	{
		// 카메라 Forward 방향의 평면에서 드래그
		FVector PlaneNormal = InActiveCamera->GetForward();

		if (ObjectPicker.IsRayCollideWithPlane(WorldRay, PlaneOrigin, PlaneNormal, MouseWorld))
		{
			// 드래그 벡터 계산
			const FVector MouseDelta = MouseWorld - Gizmo.GetDragStartMouseLocation();

			// 카메라 Right 방향으로의 드래그 거리 사용 (수평 드래그)
			const FVector CamRight = InActiveCamera->GetRight();
			const float DragDistance = MouseDelta.Dot(CamRight);

			// 스케일 민감도 조정
			const float DistanceToGizmo = (PlaneOrigin - CameraLocation).Length();
			constexpr float BaseSensitivity = 0.03f;
			const float ScaleSensitivity = BaseSensitivity * DistanceToGizmo;
			const float ScaleDelta = DragDistance * ScaleSensitivity;

			// 모든 축에 동일한 스케일 적용 (균일 스케일)
			const FVector DragStartScale = Gizmo.GetDragStartActorScale();
			const float UniformScale = max(1.0f + ScaleDelta / DragStartScale.X, MIN_SCALE_VALUE);

			FVector NewScale;
			NewScale.X = max(DragStartScale.X * UniformScale, MIN_SCALE_VALUE);
			NewScale.Y = max(DragStartScale.Y * UniformScale, MIN_SCALE_VALUE);
			NewScale.Z = max(DragStartScale.Z * UniformScale, MIN_SCALE_VALUE);

			return NewScale;
		}
		return Gizmo.GetComponentScale();
	}

	// 평면 스케일 처리
	if (Gizmo.IsPlaneDirection())
	{
		// 평면 법선과 접선 벡터
		FVector PlaneNormal = Gizmo.GetPlaneNormal();
		FVector Tangent1, Tangent2;
		Gizmo.GetPlaneTangents(Tangent1, Tangent2);

		// 로컬 좌표로 변환
		PlaneNormal = Quat.RotateVector(PlaneNormal);
		FVector WorldTangent1 = Quat.RotateVector(Tangent1);
		FVector WorldTangent2 = Quat.RotateVector(Tangent2);

		// 레이와 평면 교차점 계산
		if (ObjectPicker.IsRayCollideWithPlane(WorldRay, PlaneOrigin, PlaneNormal, MouseWorld))
		{
			// 평면 내 드래그 벡터 계산
			const FVector MouseDelta = MouseWorld - Gizmo.GetDragStartMouseLocation();

			// 두 접선 방향 드래그 거리 평균
			const float Drag1 = MouseDelta.Dot(WorldTangent1);
			const float Drag2 = MouseDelta.Dot(WorldTangent2);
			const float AvgDrag = (Drag1 + Drag2) * 0.5f;

			// 스케일 민감도 조정
			const float DistanceToGizmo = (PlaneOrigin - CameraLocation).Length();
			constexpr float BaseSensitivity = 0.03f;
			const float ScaleSensitivity = BaseSensitivity * DistanceToGizmo;
			const float ScaleDelta = AvgDrag * ScaleSensitivity;

			const FVector DragStartScale = Gizmo.GetDragStartActorScale();
			FVector NewScale = DragStartScale;

			// 평면의 두 축에 동일한 스케일 적용
			if (abs(Tangent1.X) > 0.5f)
			{
				NewScale.X = max(DragStartScale.X + ScaleDelta, MIN_SCALE_VALUE);
			}
			if (abs(Tangent1.Y) > 0.5f)
			{
				NewScale.Y = max(DragStartScale.Y + ScaleDelta, MIN_SCALE_VALUE);
			}
			if (abs(Tangent1.Z) > 0.5f)
			{
				NewScale.Z = max(DragStartScale.Z + ScaleDelta, MIN_SCALE_VALUE);
			}
			if (abs(Tangent2.X) > 0.5f)
			{
				NewScale.X = max(DragStartScale.X + ScaleDelta, MIN_SCALE_VALUE);
			}
			if (abs(Tangent2.Y) > 0.5f)
			{
				NewScale.Y = max(DragStartScale.Y + ScaleDelta, MIN_SCALE_VALUE);
			}
			if (abs(Tangent2.Z) > 0.5f)
			{
				NewScale.Z = max(DragStartScale.Z + ScaleDelta, MIN_SCALE_VALUE);
			}

			return NewScale;
		}
		return Gizmo.GetComponentScale();
	}

	// 축 스케일 처리 (기존 로직)
	FVector CardinalAxis = Gizmo.GetGizmoAxis();
	FVector GizmoAxis = Quat.RotateVector(CardinalAxis);

	// 카메라 방향 벡터를 사용하여 안정적인 평면 계산
	const FVector CamForward = InActiveCamera->GetForward();
	const FVector CamRight = InActiveCamera->GetRight();
	const FVector CamUp = InActiveCamera->GetUp();

	// GizmoAxis에 가장 수직인 카메라 벡터 선택
	FVector PlaneVector;
	const float DotRight = abs(GizmoAxis.Dot(CamRight));
	const float DotUp = abs(GizmoAxis.Dot(CamUp));

	if (DotRight < DotUp)
	{
		PlaneVector = CamRight;
	}
	else
	{
		PlaneVector = CamUp;
	}

	FVector PlaneNormal = GizmoAxis.Cross(PlaneVector);
	PlaneNormal.Normalize();

	// PlaneNormal이 카메라를 향하도록 보장 (드래그 방향 일관성)
	const FVector ToCamera = (CameraLocation - PlaneOrigin).GetNormalized();
	if (PlaneNormal.Dot(ToCamera) < 0.0f)
	{
		PlaneNormal = -PlaneNormal;
	}

	if (ObjectPicker.IsRayCollideWithPlane(WorldRay, PlaneOrigin, PlaneNormal, MouseWorld))
	{
		// UE 방식을 사용, 드래그 거리에 비례하여 스케일 변화
		const FVector MouseDelta = MouseWorld - Gizmo.GetDragStartMouseLocation();
		const float AxisDragDistance = MouseDelta.Dot(GizmoAxis);

		// 카메라 거리에 따른 스케일 민감도 조정
		const float DistanceToGizmo = (PlaneOrigin - CameraLocation).Length();

		// 거리에 비례한 민감도, 기본 배율 적용
		// 가까울수록 정밀하게, 멀수록 빠르게 조정
		constexpr float BaseSensitivity = 0.03f;  // 기본 민감도
		const float ScaleSensitivity = BaseSensitivity * DistanceToGizmo;
		const float ScaleDelta = AxisDragDistance * ScaleSensitivity;

		const FVector DragStartScale = Gizmo.GetDragStartActorScale();
		FVector NewScale;

		if (Gizmo.GetSelectedComponent()->IsUniformScale())
		{
			// Uniform Scale: 모든 축에 동일한 스케일 델타 적용
			const float UniformDelta = ScaleDelta;
			NewScale = DragStartScale + FVector(UniformDelta, UniformDelta, UniformDelta);

			// 모든 축이 최소값 이상이 되도록 보장
			NewScale.X = std::max(NewScale.X, MIN_SCALE_VALUE);
			NewScale.Y = std::max(NewScale.Y, MIN_SCALE_VALUE);
			NewScale.Z = std::max(NewScale.Z, MIN_SCALE_VALUE);
		}
		else
		{
			// Non-uniform Scale: 선택한 축에만 스케일 델타 적용
			NewScale = DragStartScale;

			// X축 (1,0,0)
			if (abs(CardinalAxis.X) > 0.5f)
			{
				NewScale.X = std::max(DragStartScale.X + ScaleDelta, MIN_SCALE_VALUE);
			}
			// Y축 (0,1,0)
			else if (abs(CardinalAxis.Y) > 0.5f)
			{
				NewScale.Y = std::max(DragStartScale.Y + ScaleDelta, MIN_SCALE_VALUE);
			}
			// Z축 (0,0,1)
			else if (abs(CardinalAxis.Z) > 0.5f)
			{
				NewScale.Z = std::max(DragStartScale.Z + ScaleDelta, MIN_SCALE_VALUE);
			}
		}

		return NewScale;
	}
	return Gizmo.GetComponentScale();
}

void UEditor::SelectActor(AActor* InActor)
{
	if (InActor == SelectedActor) return;

	// 이전 선택 해제 (모든 컴포넌트)
	if (SelectedActor)
	{
		for (UActorComponent* Component : SelectedActor->GetOwnedComponents())
		{
			if (Component)
			{
				Component->OnDeselected();
			}
		}
	}

	SelectedActor = InActor;

	if (SelectedActor)
	{
		// Actor 선택 시 모든 컴포넌트 하이라이팅
		for (UActorComponent* Component : SelectedActor->GetOwnedComponents())
		{
			if (Component)
			{
				Component->OnSelected();
			}
		}

		// Gizmo는 RootComponent에 부착
		SelectedComponent = InActor->GetRootComponent();
		Gizmo.SetSelectedComponent(Cast<USceneComponent>(SelectedComponent));
		UUIManager::GetInstance().OnSelectedComponentChanged(SelectedComponent);
	}
	else
	{
		SelectedComponent = nullptr;
		Gizmo.SetSelectedComponent(nullptr);
		UUIManager::GetInstance().OnSelectedComponentChanged(nullptr);
	}
}

void UEditor::SelectActorAndComponent(AActor* InActor, UActorComponent* InComponent)
{
	// 이전 Actor의 모든 컴포넌트 선택 해제
	if (SelectedActor && SelectedActor != InActor)
	{
		for (UActorComponent* Component : SelectedActor->GetOwnedComponents())
		{
			if (Component)
			{
				Component->OnDeselected();
			}
		}
	}
	else if (SelectedActor == InActor)
	{
		// 같은 Actor 내에서 컴포넌트만 변경: 기존 모든 컴포넌트 해제
		for (UActorComponent* Component : SelectedActor->GetOwnedComponents())
		{
			if (Component)
			{
				Component->OnDeselected();
			}
		}
	}

	SelectedActor = InActor;
	SelectComponent(InComponent);
}

void UEditor::SelectComponent(UActorComponent* InComponent)
{
	if (InComponent == SelectedComponent) return;

	// Component 선택 시 단일 컴포넌트만 하이라이팅
	if (SelectedComponent)
	{
		SelectedComponent->OnDeselected();
	}

	SelectedComponent = InComponent;
	if (SelectedComponent)
	{
		SelectedComponent->OnSelected();
		Gizmo.SetSelectedComponent(Cast<USceneComponent>(SelectedComponent));
	}
	else
	{
		Gizmo.SetSelectedComponent(nullptr);
	}
	UUIManager::GetInstance().OnSelectedComponentChanged(SelectedComponent);
}

bool UEditor::GetComponentFocusTarget(UActorComponent* Component, FVector& OutCenter, float& OutRadius)
{
	if (!Component)
	{
		UE_LOG_WARNING("Editor: GetComponentFocusTarget: Component is null");
		return false;
	}

	USceneComponent* SceneComp = Cast<USceneComponent>(Component);
	if (!SceneComp)
	{
		UE_LOG_WARNING("Editor: GetComponentFocusTarget: Not a SceneComponent");
		return false;
	}

	// LightComponent: 10x10x10 박스 가정한 AABB
	if (ULightComponent* LightComp = Cast<ULightComponent>(SceneComp))
	{
		OutCenter = LightComp->GetWorldLocation();
		// 10x10x10 박스의 반경 = sqrt(10^2 + 10^2 + 10^2) / 2 = 8.66
		OutRadius = 8.66f;
		return true;
	}

	// PrimitiveComponent: AABB 기반 계산
	if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(SceneComp))
	{
		FVector Min, Max;
		PrimComp->GetWorldAABB(Min, Max);
		OutCenter = (Min + Max) * 0.5f;

		FVector Size = Max - Min;
		OutRadius = Size.Length() * 0.5f;

		// 최소 반경 보장 (너무 작은 오브젝트 대응)
		OutRadius = max(OutRadius, 10.0f);
	}
	// SceneComponent: WorldLocation 기반
	else
	{
		OutCenter = SceneComp->GetWorldLocation();
		OutRadius = 30.0f;
	}

	return true;
}

bool UEditor::GetActorFocusTarget(AActor* Actor, FVector& OutCenter, float& OutRadius)
{
	if (!Actor)
	{
		UE_LOG_WARNING("Editor: GetActorFocusTarget: Actor is null");
		return false;
	}

	// Actor의 모든 Primitive Component를 수집하여 전체 AABB 계산
	TArray<UPrimitiveComponent*> PrimitiveComponents;

	for (UActorComponent* Comp : Actor->GetOwnedComponents())
	{
		if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Comp))
		{
			// Visualization Component는 제외 (실제 렌더링되는 메쉬만 포함)
			if (PrimComp->IsVisualizationComponent())
			{
				continue;
			}
			PrimitiveComponents.push_back(PrimComp);
		}
	}

	// Primitive가 없으면 LightComponent 찾기 (Light Actor 처리)
	if (PrimitiveComponents.empty())
	{
		for (UActorComponent* Comp : Actor->GetOwnedComponents())
		{
			if (ULightComponent* LightComp = Cast<ULightComponent>(Comp))
			{
				FVector LightLoc = LightComp->GetWorldLocation();
				// 10x10x10 박스 가정
				OutCenter = LightLoc;
				OutRadius = 8.66f; // sqrt(10^2 + 10^2 + 10^2) / 2
				return true;
			}
		}

		UE_LOG_WARNING("Editor: GetActorFocusTarget: No valid component found");
		return false;
	}

	// 모든 컴포넌트의 AABB를 합쳐서 전체 바운딩 계산
	FVector GlobalMin(FLT_MAX, FLT_MAX, FLT_MAX);
	FVector GlobalMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	for (UPrimitiveComponent* Prim : PrimitiveComponents)
	{
		FVector Min, Max;
		Prim->GetWorldAABB(Min, Max);

		GlobalMin.X = min(GlobalMin.X, Min.X);
		GlobalMin.Y = min(GlobalMin.Y, Min.Y);
		GlobalMin.Z = min(GlobalMin.Z, Min.Z);

		GlobalMax.X = max(GlobalMax.X, Max.X);
		GlobalMax.Y = max(GlobalMax.Y, Max.Y);
		GlobalMax.Z = max(GlobalMax.Z, Max.Z);
	}

	OutCenter = (GlobalMin + GlobalMax) * 0.5f;

	FVector Size = GlobalMax - GlobalMin;
	OutRadius = Size.Length() * 0.5f;

	UE_LOG("Editor: GetActorFocusTarget: Mesh Actor Center=(%.1f,%.1f,%.1f) Radius=%.1f",
		OutCenter.X, OutCenter.Y, OutCenter.Z, OutRadius);
	return true;
}

void UEditor::FocusOnSelectedActor()
{
	auto& ViewportManager = UViewportManager::GetInstance();
	auto& Viewports = ViewportManager.GetViewports();
	auto& Clients = ViewportManager.GetClients();

	if (Viewports.empty() || Clients.empty())
	{
		return;
	}

	const int32 ViewportCount = static_cast<int32>(Viewports.size());

	// 마지막 클릭한 뷰포트의 카메라 타입 가져오기
	const int32 LastClickedIdx = ViewportManager.GetLastClickedViewportIndex();
	if (LastClickedIdx < 0 || LastClickedIdx >= ViewportCount || !Clients[LastClickedIdx])
	{
		return;
	}

	UCamera* LastClickedCam = Clients[LastClickedIdx]->GetCamera();
	if (!LastClickedCam)
	{
		return;
	}

	const ECameraType LastClickedCameraType = LastClickedCam->GetCameraType();
	const bool bIsOrtho = LastClickedCameraType == ECameraType::ECT_Orthographic;

	FVector Center;
	float BoundingRadius;
	bool bSuccess = false;

	// Component 선택 모드: 선택된 Component에 포커싱
	if (!bIsActorSelected && SelectedComponent)
	{
		if (bIsOrtho)
		{
			// Ortho 뷰: 로컬 원점(0,0,0) 사용
			if (USceneComponent* SceneComp = Cast<USceneComponent>(SelectedComponent))
			{
				Center = SceneComp->GetWorldLocation();
				BoundingRadius = 50.0f;
				bSuccess = true;
			}
		}
		else
		{
			// Perspective 뷰: AABB 중심 사용
			bSuccess = GetComponentFocusTarget(SelectedComponent, Center, BoundingRadius);
		}
	}

	// Actor 선택 모드: Actor 전체에 포커싱
	else if (SelectedActor)
	{
		if (bIsOrtho)
		{
			// Ortho 뷰: RootComponent의 월드 위치 사용
			if (USceneComponent* RootComp = SelectedActor->GetRootComponent())
			{
				Center = RootComp->GetWorldLocation();
				BoundingRadius = 100.0f;
				bSuccess = true;
			}
		}
		else
		{
			// Perspective 뷰: AABB 중심 사용
			bSuccess = GetActorFocusTarget(SelectedActor, Center, BoundingRadius);
		}
	}

	if (!bSuccess)
	{
		return;
	}

	CameraStartLocation.resize(ViewportCount);
	CameraStartRotation.resize(ViewportCount);
	CameraTargetLocation.resize(ViewportCount);
	CameraTargetRotation.resize(ViewportCount);
	OrthoZoomStart.resize(ViewportCount);
	OrthoZoomTarget.resize(ViewportCount);

	AnimatingCameraType = LastClickedCameraType;

	for (int32 i = 0; i < ViewportCount; ++i)
	{
		if (!Clients[i]) continue;
		UCamera* Cam = Clients[i]->GetCamera();
		if (!Cam) continue;

		// 모든 카메라의 현재 상태 저장 (애니메이션 필터링은 나중에)
		CameraStartLocation[i] = Cam->GetLocation();
		CameraStartRotation[i] = Cam->GetRotation();

		// 오쏘 카메라면 현재 줌 값도 저장
		if (Cam->GetCameraType() == ECameraType::ECT_Orthographic)
		{
			OrthoZoomStart[i] = Cam->GetOrthoZoom();
		}

		// 마지막 클릭한 뷰포트와 동일한 카메라 타입만 목표 위치 계산
		if (Cam->GetCameraType() != LastClickedCameraType)
		{
			// 타입이 다르면 현재 위치를 목표로 (애니메이션 안 함)
			CameraTargetLocation[i] = CameraStartLocation[i];
			CameraTargetRotation[i] = CameraStartRotation[i];
			OrthoZoomTarget[i] = OrthoZoomStart[i];
			continue;
		}

		if (Cam->GetCameraType() == ECameraType::ECT_Perspective)
		{
			// 카메라의 Forward 벡터를 직접 사용 (오일러각 대신)
			const FVector Forward = Cam->GetForward();

			const float FovY = Cam->GetFovY();
			const float HalfFovRadian = FVector::GetDegreeToRadian(FovY * 0.5f);

			// BoundingRadius 기준으로 거리 계산
			float Distance = BoundingRadius / sinf(HalfFovRadian);

			// EditorIcon이나 Billboard는 작은 스프라이트이므로 더 가까이
			if (UEditorIconComponent* IconComp = Cast<UEditorIconComponent>(SelectedComponent))
			{
				Distance = min(Distance, 200.0f);
			}
			else if (UBillBoardComponent* BillboardComp = Cast<UBillBoardComponent>(SelectedComponent))
			{
				Distance = min(Distance, 200.0f);
			}

			CameraTargetLocation[i] = Center - Forward * Distance;

			// 목표 회전은 현재 회전 유지 (카메라 각도가 바뀌지 않음)
			CameraTargetRotation[i] = Cam->GetRotation();

			// Perspective는 줌 애니메이션 없음
			OrthoZoomTarget[i] = OrthoZoomStart[i];
		}
		else
		{
			// Orthographic: 물체 중심으로 카메라 이동 + 줌 애니메이션
			const FVector Forward = Cam->GetForward();

			// 현재 카메라에서 물체 중심까지의 Forward 방향 투영 거리 계산
			const FVector ToCenterVec = Center - CameraStartLocation[i];
			const float DistanceToCenter = ToCenterVec.Dot(Forward);

			// 물체 중심을 바라보도록 카메라 위치 조정 (Forward 방향 유지)
			CameraTargetLocation[i] = Center - Forward * abs(DistanceToCenter);

			// 회전은 현재 유지
			CameraTargetRotation[i] = CameraStartRotation[i];

			// 오쏘 줌 목표값 설정 (애니메이션으로 전환)
			OrthoZoomTarget[i] = 500.0f;
		}
	}

	// 애니메이션 시작
	bIsCameraAnimating = true;
	CameraAnimationTime = 0.0f;
}

void UEditor::UpdateCameraAnimation()
{
	if (!bIsCameraAnimating)
	{
		return;
	}

	auto& ViewportManager = UViewportManager::GetInstance();
	auto& Clients = ViewportManager.GetClients();
	const UInputManager& Input = UInputManager::GetInstance();

	if (Clients.empty())
	{
		bIsCameraAnimating = false;
		return;
	}

	// 우클릭 드래그 시작 시 애니메이션 중단
	if (Input.IsKeyPressed(EKeyInput::MouseRight))
	{
		bIsCameraAnimating = false;
		return;
	}

	CameraAnimationTime += DT;
	float Progress = CameraAnimationTime / CAMERA_ANIMATION_DURATION;

	bool bAnimationCompleted = false;
	if (Progress >= 1.0f)
	{
		Progress = 1.0f;
		bIsCameraAnimating = false;
		bAnimationCompleted = true;
	}

	float SmoothProgress;
	if (Progress < 0.5f)
	{
		SmoothProgress = 8.0f * Progress * Progress * Progress * Progress;
	}
	else
	{
		float ProgressFromEnd = Progress - 1.0f;
		SmoothProgress = 1.0f - 8.0f * ProgressFromEnd * ProgressFromEnd * ProgressFromEnd * ProgressFromEnd;
	}

	const size_t AnimationVectorSize = CameraStartLocation.size();
	for (int Index = 0; Index < Clients.size() && Index < AnimationVectorSize; ++Index)
	{
		if (!Clients[Index]) continue;
		UCamera* Cam = Clients[Index]->GetCamera();
		if (!Cam) continue;

		// 애니메이션 시작 시 결정된 카메라 타입과 일치하는 경우만 처리
		if (Cam->GetCameraType() != AnimatingCameraType)
		{
			continue;
		}

		FVector CurrentLocation = CameraStartLocation[Index] + (CameraTargetLocation[Index] - CameraStartLocation[Index]) * SmoothProgress;
		Cam->SetLocation(CurrentLocation);

		// Perspective 카메라만 회전 애니메이션 적용
		// Orthographic 카메라는 회전이 고정되어야 함
		if (Cam->GetCameraType() == ECameraType::ECT_Perspective)
		{
			FVector CurrentRotation = CameraStartRotation[Index] + (CameraTargetRotation[Index] - CameraStartRotation[Index]) * SmoothProgress;
			Cam->SetRotation(CurrentRotation);
		}
		// Orthographic 카메라는 줌 애니메이션 적용
		else if (Cam->GetCameraType() == ECameraType::ECT_Orthographic)
		{
			float CurrentZoom = Lerp<float>(OrthoZoomStart[Index], OrthoZoomTarget[Index], SmoothProgress);
			Cam->SetOrthoZoom(CurrentZoom);
		}
	}

	// 애니메이션 완료 시 오쏘 뷰의 InitialOffsets, SharedOrthoZoom 및 공유 센터 업데이트
	if (bAnimationCompleted && AnimatingCameraType == ECameraType::ECT_Orthographic)
	{
		// SharedOrthoZoom 업데이트 (목표 줌 값으로 설정)
		ViewportManager.SetSharedOrthoZoom(500.0f);

		// 먼저 애니메이션 타겟 위치 기반으로 새로운 공유 센터 계산
		// 첫 번째 오쏘 뷰를 기준으로 공유 센터 설정
		FVector NewSharedCenter = FVector::ZeroVector();
		bool bCenterCalculated = false;

		for (int Index = 0; Index < Clients.size(); ++Index)
		{
			if (!Clients[Index]) continue;
			UCamera* Cam = Clients[Index]->GetCamera();
			if (!Cam || Cam->GetCameraType() != ECameraType::ECT_Orthographic) continue;

			// ViewType에 따른 InitialOffsets 인덱스 결정
			int32 OrthoIdx = -1;
			switch (Clients[Index]->GetViewType())
			{
			case EViewType::OrthoTop: OrthoIdx = 0; break;
			case EViewType::OrthoBottom: OrthoIdx = 1; break;
			case EViewType::OrthoLeft: OrthoIdx = 2; break;
			case EViewType::OrthoRight: OrthoIdx = 3; break;
			case EViewType::OrthoFront: OrthoIdx = 4; break;
			case EViewType::OrthoBack: OrthoIdx = 5; break;
			}

			if (OrthoIdx >= 0 && OrthoIdx < ViewportManager.GetInitialOffsets().size())
			{
				const FVector& OldOffset = ViewportManager.GetInitialOffsets()[OrthoIdx];
				const FVector NewLocation = Cam->GetLocation();

				// 첫 번째 오쏘 뷰 기준으로 새로운 공유 센터 계산
				if (!bCenterCalculated)
				{
					NewSharedCenter = NewLocation - OldOffset;
					bCenterCalculated = true;
				}

				// 새로운 오프셋 = 새 위치 - 새 공유 센터
				FVector NewOffset = NewLocation - NewSharedCenter;
				ViewportManager.UpdateInitialOffset(OrthoIdx, NewOffset);
			}
		}

		// 공유 센터 업데이트
		if (bCenterCalculated)
		{
			ViewportManager.SetOrthoGraphicCameraPoint(NewSharedCenter);
		}
	}
}

/**
 * @brief Component 복사 (같은 Actor 내에 추가)
 * @note 단일 Component만 복사하며, child component는 복사하지 않음
 * @param InSourceComponent 복사할 원본 Component
 * @param InParentActor 복사된 Component가 추가될 Actor
 * @return 복사된 Component (실패 시 nullptr)
 */
UActorComponent* UEditor::DuplicateComponent(UActorComponent* InSourceComponent, AActor* InParentActor)
{
	if (!InSourceComponent || !InParentActor)
	{
		return nullptr;
	}

	// Component 복사
	UActorComponent* NewComponent = Cast<UActorComponent>(InSourceComponent->Duplicate());
	if (!NewComponent)
	{
		return nullptr;
	}

	// Owner 설정 및 Parent Actor에 등록
	NewComponent->SetOwner(InParentActor);
	InParentActor->GetOwnedComponents().push_back(NewComponent);

	// DuplicateSubObjects 호출하여 서브 객체 복사
	InSourceComponent->CallDuplicateSubObjects(NewComponent);

	// SceneComponent인 경우 계층 구조 설정
	USceneComponent* NewSceneComponent = Cast<USceneComponent>(NewComponent);
	if (NewSceneComponent)
	{
		USceneComponent* SourceSceneComponent = Cast<USceneComponent>(InSourceComponent);
		if (SourceSceneComponent && SourceSceneComponent->GetAttachParent())
		{
			// 원본 Component의 부모에 새 Component도 부착
			NewSceneComponent->AttachToComponent(SourceSceneComponent->GetAttachParent());
		}
		else
		{
			// 부모가 없으면 Root Component에 부착
			if (InParentActor->GetRootComponent())
			{
				NewSceneComponent->AttachToComponent(InParentActor->GetRootComponent());
			}
		}
	}

	// Component 등록
	InParentActor->RegisterComponent(NewComponent);

	return NewComponent;
}

/**
 * @brief Actor 전체 복사
 * @param InSourceActor 복사할 원본 Actor
 * @return 복사된 Actor (실패 시 nullptr)
 */
AActor* UEditor::DuplicateActor(AActor* InSourceActor)
{
	if (!InSourceActor)
	{
		return nullptr;
	}

	// EditorWorld 가져오기
	UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
	if (!EditorWorld)
	{
		return nullptr;
	}

	// Level 가져오기
	ULevel* CurrentLevel = EditorWorld->GetLevel();
	if (!CurrentLevel)
	{
		return nullptr;
	}

	// Actor 전체 복사 (EditorOnly Component 포함)
	AActor* NewActor = Cast<AActor>(InSourceActor->DuplicateForEditor());
	if (!NewActor)
	{
		return nullptr;
	}

	// Outer 설정 (Level이 Actor의 Outer, Actor가 Component들의 Outer)
	NewActor->SetOuter(CurrentLevel);
	for (UActorComponent* Component : NewActor->GetOwnedComponents())
	{
		if (Component)
		{
			Component->SetOuter(NewActor);
		}
	}

	// Level에 Actor 추가
	CurrentLevel->AddActorToLevel(NewActor);
	CurrentLevel->AddLevelComponent(NewActor);

	// BeginPlay 호출 (에디터에서 생성된 Actor는 즉시 활성화)
	NewActor->BeginPlay();

	return NewActor;
}

/**
 * @brief Pilot Mode 진입 함수
 */
void UEditor::TogglePilotMode()
{
	// 진입 조건 검사
	if (!SelectedActor)
	{
		UE_LOG_WARNING("Pilot Mode: No actor selected");
		return;
	}

	if (!bIsActorSelected)
	{
		UE_LOG_WARNING("Pilot Mode: Component selection mode. Switch to Actor selection first");
		return;
	}

	// 마지막으로 클릭한 뷰포트의 카메라 사용
	UViewportManager& ViewportManager = UViewportManager::GetInstance();
	auto& Clients = ViewportManager.GetClients();
	const int32 LastClickedIdx = ViewportManager.GetLastClickedViewportIndex();

	if (LastClickedIdx < 0 || LastClickedIdx >= static_cast<int32>(Clients.size()))
	{
		UE_LOG_WARNING("Pilot Mode: Invalid viewport index");
		return;
	}

	FViewportClient* TargetClient = Clients[LastClickedIdx];
	if (!TargetClient || !TargetClient->GetCamera())
	{
		UE_LOG_WARNING("Pilot Mode: Target viewport has no camera");
		return;
	}

	// Pilot Mode 진입
	bIsPilotMode = true;
	PilotedActor = SelectedActor;
	PilotModeViewportIndex = LastClickedIdx;

	// 현재 카메라 위치 저장, 이후 해제 시 복원 예정
	UCamera* Cam = TargetClient->GetCamera();
	PilotModeStartCameraLocation = Cam->GetLocation();
	PilotModeStartCameraRotation = Cam->GetRotation();

	// Actor의 Transform을 카메라에 적용
	if (USceneComponent* RootComp = PilotedActor->GetRootComponent())
	{
		FVector ActorLocation = RootComp->GetWorldLocation();
		FQuaternion ActorRotationQuat = RootComp->GetWorldRotationAsQuaternion();

		Cam->SetLocation(ActorLocation);
		Cam->SetRotationQuat(ActorRotationQuat);

		// 기즈모를 원래 Actor 위치에 고정
		PilotModeFixedGizmoLocation = ActorLocation;
		Gizmo.SetFixedLocation(PilotModeFixedGizmoLocation);

		UE_LOG_INFO("Pilot Mode: Entered (Actor: %s)", PilotedActor->GetName().ToString().data());
	}
}

/**
 * @brief Pilot Mode 업데이트
 */
void UEditor::UpdatePilotMode()
{
	if (!bIsPilotMode || !PilotedActor)
	{
		return;
	}

	// Actor가 삭제되었는지 확인
	if (PilotedActor->IsPendingDestroy())
	{
		ExitPilotMode();
		return;
	}

	// 카메라의 현재 Transform을 Actor에 적용
	UViewportManager& ViewportManager = UViewportManager::GetInstance();
	auto& Clients = ViewportManager.GetClients();

	if (PilotModeViewportIndex >= 0 && PilotModeViewportIndex < static_cast<int32>(Clients.size()))
	{
		FViewportClient* PilotClient = Clients[PilotModeViewportIndex];
		if (PilotClient && PilotClient->GetCamera())
		{
			UCamera* Cam = PilotClient->GetCamera();
			USceneComponent* RootComp = PilotedActor->GetRootComponent();

			if (RootComp)
			{
				FVector CameraLocation = Cam->GetLocation();
				FQuaternion CameraRotationQuat = Cam->GetRotationQuat();

				// Actor에 카메라 Transform 직접 적용 (Camera가 내부적으로 Quaternion 사용)
				RootComp->SetWorldLocation(CameraLocation);
				RootComp->SetWorldRotation(CameraRotationQuat);
			}
		}
	}
}

/**
 * @brief Pilot Mode 해제
 */
void UEditor::ExitPilotMode()
{
	if (!bIsPilotMode)
	{
		return;
	}

	// 카메라를 시작 위치로 복원
	UViewportManager& ViewportManager = UViewportManager::GetInstance();
	auto& Clients = ViewportManager.GetClients();

	if (PilotModeViewportIndex >= 0 && PilotModeViewportIndex < static_cast<int32>(Clients.size()))
	{
		FViewportClient* PilotClient = Clients[PilotModeViewportIndex];
		if (PilotClient && PilotClient->GetCamera())
		{
			UCamera* Cam = PilotClient->GetCamera();
			Cam->SetLocation(PilotModeStartCameraLocation);
			Cam->SetRotation(PilotModeStartCameraRotation);
		}
	}

	if (PilotedActor)
	{
		UE_LOG_INFO("Pilot Mode: Exited (Actor: %s)", PilotedActor->GetName().ToString().data());
	}

	// 기즈모 고정 위치 해제
	Gizmo.ClearFixedLocation();

	bIsPilotMode = false;
	PilotedActor = nullptr;
	PilotModeViewportIndex = -1;
}

/**
 * @brief Pilot Mode 종료 요청을 위한 외부 호출 함수
 */
void UEditor::RequestExitPilotMode()
{
	ExitPilotMode();
}
