------------------------------------------------------------------
-- [사전 정의된 전역 변수]
-- Owner: 이 스크립트 컴포넌트를 소유한 C++ Actor (AActor) 객체입니다.
------------------------------------------------------------------

---
-- 게임이 시작되거나 액터가 스폰될 때 1회 호출됩니다.
---
function BeginPlay()
end

---
-- 매 프레임 호출됩니다.
-- @param dt (float): 이전 프레임으로부터 경과한 시간 (Delta Time)
---
function Tick(dt)
    local NewLocation = Owner.Location + (FVector(1, 0, 0) * dt)
    Owner.Location = NewLocation
end

---
-- 게임이 종료되거나 액터가 파괴될 때 1회 호출됩니다.
---
function EndPlay()
end

------------------------------------------------------------------
-- [Delegate 이벤트 콜백]
-- Owner Actor의 Delegate가 Broadcast될 때 자동으로 호출됩니다.
-- 필요한 이벤트만 정의하면 됩니다.
------------------------------------------------------------------

---
-- 다른 액터와 오버랩이 시작될 때 호출됩니다.
-- @param overlappedActor (AActor): 오버랩된 액터 (Owner)
-- @param otherActor (AActor): 오버랩을 일으킨 다른 액터
---
function OnActorBeginOverlap(overlappedActor, otherActor)
    -- Log("Overlap started with: " .. otherActor.Name)
end

---
-- 다른 액터와 오버랩이 종료될 때 호출됩니다.
-- @param overlappedActor (AActor): 오버랩된 액터 (Owner)
-- @param otherActor (AActor): 오버랩을 종료한 다른 액터
---
function OnActorEndOverlap(overlappedActor, otherActor)
    -- Log("Overlap ended with: " .. otherActor.Name)
end

---
-- 다른 액터와 충돌(Hit)이 발생했을 때 호출됩니다.
-- 주의 : 아직 물리 충돌이 구현되지 않았으므로 작동하지 않음
-- @param hitActor (AActor): 충돌한 액터 (Owner)
-- @param otherActor (AActor): 충돌한 다른 액터
-- @param normalImpulse (FVector): 충돌 시 법선 방향 힘
-- @param hit (FHitResult): 충돌 정보 (위치, 법선 등)
---
function OnActorHit(hitActor, otherActor, normalImpulse, hit)
    -- Log("Hit by: " .. otherActor.Name)
end
