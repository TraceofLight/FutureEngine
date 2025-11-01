------------------------------------------------------------------
-- CharacterGravity.lua
-- Box 충돌 + 중력 시스템 샘플 코드
------------------------------------------------------------------
-- [사용법]
-- 1. Actor에 BoxComponent 추가 (크기: 4x4x9)
-- 2. ScriptComponent 추가 후 이 스크립트 할당
-- 3. 바닥에 정적 Box 배치 (테스트용)
-- 4. 실행하면 캐릭터가 낙하 → 바닥 충돌 → 정지
------------------------------------------------------------------
-- [사전 정의된 전역 변수]
-- Owner: 이 스크립트 컴포넌트를 소유한 C++ Actor (AActor) 객체
------------------------------------------------------------------

-- 지역 변수 (상태 저장)
local velocity = FVector(0, 0, 0)  -- 현재 속도 (cm/s)
local isGrounded = false           -- 바닥에 닿았는지 여부
local gravityScale = 980           -- 중력 가속도 (cm/s²)

---
-- 게임이 시작되거나 액터가 스폰될 때 1회 호출됩니다.
---
function BeginPlay()
    Log("=== CharacterGravity Initialized ===")
    Log("Owner: " .. Owner.Name)
    Log("Gravity: " .. gravityScale .. " cm/s²")
end

---
-- 매 프레임 호출됩니다.
-- @param dt (float): 이전 프레임으로부터 경과한 시간 (Delta Time)
---
function Tick(dt)
    -- 1. 중력 적용 (바닥에 닿지 않았을 때만)
    if not isGrounded then
        velocity.Z = velocity.Z - gravityScale * dt

        -- 낙하 중일 때 로그 (디버깅용)
        if velocity.Z < -100 then
            -- Log(string.format("Falling: velocity.Z = %.2f", velocity.Z))
        end
    end

    -- 2. 이동 (속도 * 시간)
    local currentPos = Owner.Location
    local delta = velocity * dt
    local newPos = currentPos + delta
    Owner.Location = newPos

    -- 3. 바닥 판정 초기화 (매 프레임마다 재설정)
    --    PrimitiveComponent는 매 프레임 UpdateOverlaps()를 실행 (Unreal-style)
    --    바닥과 계속 접촉 중이면 OnActorHit이 매 프레임 isGrounded를 true로 설정
    --    공중에 있으면 OnActorHit이 호출되지 않아 isGrounded는 false로 유지
    --    이를 통해 안정적인 바닥 감지 및 떨림 방지 가능
    isGrounded = false
end

---
-- 다른 액터와 충돌(Hit)이 발생했을 때 호출됩니다.
-- NOTE: 충돌 중인 동안 매 프레임 호출됩니다 (Unreal-style)
--       OnActorBeginOverlap과 달리, 접촉이 유지되는 한 계속 발동
-- @param hitActor (AActor): 충돌한 액터 (Owner)
-- @param otherActor (AActor): 충돌한 다른 액터
-- @param normalImpulse (FVector): 충돌 시 법선 방향 힘 (현재 미사용)
-- @param hit (FHitResult): 충돌 정보 (Normal, PenetrationDepth, Location)
---
function OnActorHit(hitActor, otherActor, normalImpulse, hit)
    Log("Collision with: " .. otherActor.Name)

    -- 충돌 정보 추출
    local normal = hit.Normal                  -- 충돌 법선 벡터
    local depth = hit.PenetrationDepth         -- 침투 깊이

    -- 디버깅: 충돌 상세 정보 출력
    Log(string.format("  Normal: (%.2f, %.2f, %.2f)", normal.X, normal.Y, normal.Z))
    Log(string.format("  Penetration Depth: %.2f", depth))

    -- === 충돌 반응 처리 ===

    -- 1. 위치 보정 (침투 제거)
    --    법선 방향으로 침투 깊이만큼 이동하여 겹침 제거
    local currentPos = Owner.Location
    local correction = normal * depth
    local correctedPos = currentPos + correction
    Owner.Location = correctedPos

    -- 2. 속도 보정 (법선 방향 성분만 제거)
    --    벽에 부딪히면 벽 방향 속도만 제거, 다른 방향은 유지
    local vDotN = velocity:Dot(normal)
    if vDotN < 0 then
        -- 충돌 방향으로 향하는 속도만 제거
        local normalComponent = normal * vDotN
        velocity = velocity - normalComponent

        -- 디버깅: 속도 보정 로그
        Log(string.format("  Velocity corrected: (%.2f, %.2f, %.2f)",
            velocity.X, velocity.Y, velocity.Z))
    end

    -- 3. 바닥 판정 (법선이 위를 향하는지 확인)
    --    Normal.Z > 0.7 ≈ 법선이 수직에서 45도 이내
    --    즉, 바닥으로 간주할 수 있는 경사
    if normal.Z > 0.7 then
        isGrounded = true
        Log("  -> Grounded!")
    end
end

---
-- 게임이 종료되거나 액터가 파괴될 때 1회 호출됩니다.
---
function EndPlay()
    Log("=== CharacterGravity Destroyed ===")
end

------------------------------------------------------------------
-- [향후 확장 가능 기능]
-- 아래 코드들은 InputManager 바인딩 후 활성화 가능
------------------------------------------------------------------

---
-- 수평 이동 추가 (WASD 입력)
-- InputManager가 전역으로 바인딩되면 사용 가능
---
--[[
function AddMovementInput(direction, scale)
    if isGrounded then
        local moveSpeed = 500  -- cm/s
        velocity.X = direction.X * scale * moveSpeed
        velocity.Y = direction.Y * scale * moveSpeed
    end
end
]]--

---
-- 점프 기능 추가 (스페이스바 입력)
---
--[[
function Jump()
    if isGrounded then
        velocity.Z = 1000  -- 점프 속도 (cm/s)
        isGrounded = false
        Log("Jump!")
    end
end
]]--

------------------------------------------------------------------
-- [테스트 시나리오]
-- 1. 에디터에서 Actor 생성
-- 2. BoxComponent 추가 (Extents: 4, 4, 9)
-- 3. ScriptComponent 추가 → CharacterGravity.lua 할당
-- 4. 바닥용 정적 Box 생성 (큰 크기로)
-- 5. 캐릭터를 공중에 배치
-- 6. 실행 → 낙하 → 충돌 → 정지 확인
------------------------------------------------------------------
