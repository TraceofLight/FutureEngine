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
