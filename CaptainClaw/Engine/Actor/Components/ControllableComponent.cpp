#include "ControllableComponent.h"
#include "../../Events/Events.h"
#include "../../Events/EventMgr.h"
#include "AnimationComponent.h"
#include "PhysicsComponent.h"
#include "RenderComponent.h"
#include "PositionComponent.h"
#include "../ActorTemplates.h"
#include "ControllerComponents/AmmoComponent.h"
#include "ControllerComponents/PowerupComponent.h"
#include "ControllerComponents/HealthComponent.h"
#include "FollowableComponent.h"

#include "../../GameApp/BaseGameApp.h"
#include "../../GameApp/BaseGameLogic.h"
#include "../../UserInterface/HumanView.h"
#include "../../Scene/SceneNodes.h"

const char* ControllableComponent::g_Name = "ControllableComponent";
const char* ClawControllableComponent::g_Name = "ClawControllableComponent";

ControllableComponent::ControllableComponent()
    :
    m_Active(false),
    m_DuckingTime(0),
    m_LookingUpTime(0),
    m_bFrozen(false),
    m_MaxJumpHeight(0)
{ }

bool ControllableComponent::VInit(TiXmlElement* data)
{
    assert(data != NULL);

    if (TiXmlElement* isActiveElement = data->FirstChildElement("IsActive"))
    {
        m_Active = std::string(isActiveElement->GetText()) == "true";
    }
    else
    {
        m_Active = false;
    }

    return VInitDelegate(data);
}

void ControllableComponent::VPostInit()
{
    shared_ptr<PhysicsComponent> pPhysicsComponent =
        MakeStrongPtr(_owner->GetComponent<PhysicsComponent>(PhysicsComponent::g_Name));
    if (pPhysicsComponent)
    {
        pPhysicsComponent->SetControllableComponent(this);
    }

    if (m_Active)
    {
        shared_ptr<EventData_Attach_Actor> pEvent(new EventData_Attach_Actor(_owner->GetGUID()));
        IEventMgr::Get()->VTriggerEvent(pEvent);
    }
}

TiXmlElement* ControllableComponent::VGenerateXml()
{
    TiXmlElement* baseElement = new TiXmlElement(VGetName());

    //

    return baseElement;
}

//=====================================================================================================================

ClawControllableComponent::ClawControllableComponent()
{
    m_pClawAnimationComponent = NULL;
    m_pRenderComponent = NULL;
    m_State = ClawState_None;
    m_LastState = ClawState_None;
    m_Direction = Direction_Right;
    m_IdleTime = 0;
    m_TakeDamageDuration = 500; // TODO: Data drive
    m_TakeDamageTimeLeftMs = 0;
    m_bIsInBossFight = false;

    IEventMgr::Get()->VAddListener(MakeDelegate(this, &ClawControllableComponent::BossFightStartedDelegate), EventData_Boss_Fight_Started::sk_EventType);
    IEventMgr::Get()->VAddListener(MakeDelegate(this, &ClawControllableComponent::BossFightEndedDelegate), EventData_Boss_Fight_Ended::sk_EventType);
}

ClawControllableComponent::~ClawControllableComponent()
{
    IEventMgr::Get()->VRemoveListener(MakeDelegate(this, &ClawControllableComponent::BossFightStartedDelegate), EventData_Boss_Fight_Started::sk_EventType);
    IEventMgr::Get()->VRemoveListener(MakeDelegate(this, &ClawControllableComponent::BossFightEndedDelegate), EventData_Boss_Fight_Ended::sk_EventType);
}

bool ClawControllableComponent::VInitDelegate(TiXmlElement* data)
{
    return true;
}

void ClawControllableComponent::VPostInit()
{
    ControllableComponent::VPostInit();

    m_pRenderComponent = MakeStrongPtr(_owner->GetComponent<ActorRenderComponent>(ActorRenderComponent::g_Name)).get();
    m_pClawAnimationComponent = MakeStrongPtr(_owner->GetComponent<AnimationComponent>(AnimationComponent::g_Name)).get();
    m_pPositionComponent = MakeStrongPtr(_owner->GetComponent<PositionComponent>(PositionComponent::g_Name)).get();
    m_pAmmoComponent = MakeStrongPtr(_owner->GetComponent<AmmoComponent>(AmmoComponent::g_Name)).get();
    m_pPowerupComponent = MakeStrongPtr(_owner->GetComponent<PowerupComponent>(PowerupComponent::g_Name)).get();
    m_pHealthComponent = MakeStrongPtr(_owner->GetComponent<HealthComponent>(HealthComponent::g_Name)).get();
    assert(m_pClawAnimationComponent);
    assert(m_pRenderComponent);
    assert(m_pPositionComponent);
    assert(m_pAmmoComponent);
    assert(m_pPowerupComponent);
    assert(m_pHealthComponent);
    m_pClawAnimationComponent->AddObserver(this);

    auto pHealthComponent = MakeStrongPtr(_owner->GetComponent<HealthComponent>(HealthComponent::g_Name));
    pHealthComponent->AddObserver(this);

    m_pPhysicsComponent = MakeStrongPtr(_owner->GetComponent<PhysicsComponent>(PhysicsComponent::g_Name)).get();

    // Sounds that play when claw takes some damage
    m_TakeDamageSoundList.push_back(SOUND_CLAW_TAKE_DAMAGE1);
    m_TakeDamageSoundList.push_back(SOUND_CLAW_TAKE_DAMAGE2);
    m_TakeDamageSoundList.push_back(SOUND_CLAW_TAKE_DAMAGE3);
    m_TakeDamageSoundList.push_back(SOUND_CLAW_TAKE_DAMAGE4);

    // Sounds that play when claw is idle for some time
    m_IdleQuoteSoundList.push_back(SOUND_CLAW_IDLE1);
    //m_IdleQuoteSoundList.push_back(SOUND_CLAW_IDLE2);
    m_IdleQuoteSoundList.push_back(SOUND_CLAW_IDLE3);
    m_IdleQuoteSoundList.push_back(SOUND_CLAW_IDLE4);
    m_IdleQuoteSoundList.push_back(SOUND_CLAW_IDLE5);
    m_IdleQuoteSoundList.push_back(SOUND_CLAW_IDLE6);
    m_IdleQuoteSoundList.push_back(SOUND_CLAW_IDLE7);
    m_IdleQuoteSoundList.push_back(SOUND_CLAW_IDLE8);
    m_IdleQuoteSoundList.push_back(SOUND_CLAW_IDLE9);
    m_IdleQuoteSoundList.push_back(SOUND_CLAW_IDLE10);
    m_IdleQuoteSoundList.push_back(SOUND_CLAW_IDLE11);
    m_IdleQuoteSoundList.push_back(SOUND_CLAW_IDLE12);

    m_pIdleQuotesSequence.reset(new PrimeSearch(m_IdleQuoteSoundList.size()));
}

void ClawControllableComponent::VPostPostInit()
{
    // So that Claw can attach to rope
    ActorFixtureDef fixtureDef;
    fixtureDef.collisionShape = "Rectangle";
    fixtureDef.fixtureType = FixtureType_RopeSensor;
    fixtureDef.size = Point(25, 25);
    fixtureDef.collisionMask = CollisionFlag_Rope;
    fixtureDef.collisionFlag = CollisionFlag_RopeSensor;
    fixtureDef.isSensor = true;

    g_pApp->GetGameLogic()->VGetGamePhysics()->VAddActorFixtureToBody(_owner->GetGUID(), &fixtureDef);
}

void ClawControllableComponent::VUpdate(uint32 msDiff)
{
    if (m_State == ClawState_Standing ||
        m_State == ClawState_Idle)
    {
        m_IdleTime += msDiff;
        if (m_IdleTime > g_pApp->GetGlobalOptions()->idleSoundQuoteIntervalMs)
        {
            // This is to prevent the same sound to play multiple times in a row
            int idleQuoteSoundIdx = m_pIdleQuotesSequence->GetNext();
            if (idleQuoteSoundIdx == -1)
            {
                idleQuoteSoundIdx = m_pIdleQuotesSequence->GetNext(true);
            }

            SoundInfo soundInfo(m_IdleQuoteSoundList[idleQuoteSoundIdx]);
            IEventMgr::Get()->VTriggerEvent(IEventDataPtr(
                new EventData_Request_Play_Sound(soundInfo)));

            shared_ptr<FollowableComponent> pExclamationMark =
                MakeStrongPtr(_owner->GetComponent<FollowableComponent>(FollowableComponent::g_Name));
            if (pExclamationMark)
            {
                pExclamationMark->Activate(2000);
            }

            m_pClawAnimationComponent->SetAnimation("idle");

            m_State = ClawState_Idle;
            m_IdleTime = 0;
        }
    }
    else
    {
        m_IdleTime = 0;
    }

    // Check if invulnerability caused by previously taking damage is gone
    if (m_TakeDamageTimeLeftMs > 0)
    {
        m_TakeDamageTimeLeftMs -= msDiff;
        if (m_TakeDamageTimeLeftMs <= 0)
        {
            if (!m_pPowerupComponent->HasPowerup(PowerupType_Invulnerability))
            {
                m_pHealthComponent->SetInvulnerable(false);
            }
            m_TakeDamageTimeLeftMs = 0;
        }
    }

    assert(g_pApp->GetHumanView() && g_pApp->GetHumanView()->GetCamera());
    shared_ptr<CameraNode> pCamera = g_pApp->GetHumanView()->GetCamera();

    if (m_DuckingTime > g_pApp->GetGlobalOptions()->startLookUpOrDownTime)
    {
        if (pCamera->GetCameraOffsetY() < g_pApp->GetGlobalOptions()->maxLookUpOrDownDistance)
        {
            // Pixels per milisecond
            double cameraOffsetSpeed = g_pApp->GetGlobalOptions()->lookUpOrDownSpeed / 1000.0;
            pCamera->AddCameraOffsetY(cameraOffsetSpeed * msDiff);
        }
    }
    else if (m_LookingUpTime > g_pApp->GetGlobalOptions()->startLookUpOrDownTime)
    {
        if (pCamera->GetCameraOffsetY() > -1.0 * g_pApp->GetGlobalOptions()->maxLookUpOrDownDistance)
        {
            // Pixels per milisecond
            double cameraOffsetSpeed = -1.0 * (g_pApp->GetGlobalOptions()->lookUpOrDownSpeed / 1000.0);
            pCamera->AddCameraOffsetY(cameraOffsetSpeed * msDiff);
        }
        m_pClawAnimationComponent->SetAnimation("lookup");
    }
    else
    {
        if (m_State != ClawState_Ducking)
        {
            m_DuckingTime = 0;
        }
        if (m_State != ClawState_Standing)
        {
            m_LookingUpTime = 0;
        }

        if (fabs(pCamera->GetCameraOffsetY()) > DBL_EPSILON)
        {
            double cameraOffsetSpeed = ((double)g_pApp->GetGlobalOptions()->lookUpOrDownSpeed * 2.0) / 1000.0;
            if (pCamera->GetCameraOffsetY() > DBL_EPSILON)
            {
                // Camera should move back up
                double cameraMovePx = -1.0 * cameraOffsetSpeed * msDiff;
                pCamera->AddCameraOffsetY(cameraMovePx);
                if (pCamera->GetCameraOffsetY() < DBL_EPSILON)
                {
                    pCamera->SetCameraOffsetY(0.0);
                }
            }
            else if (pCamera->GetCameraOffsetY() < (-1.0 * DBL_EPSILON))
            {
                // Camera should move back down
                double cameraMovePx = cameraOffsetSpeed * msDiff;
                pCamera->AddCameraOffsetY(cameraMovePx);
                if (pCamera->GetCameraOffsetY() > DBL_EPSILON)
                {
                    pCamera->SetCameraOffsetY(0.0);
                }
            }
        }
    }

    if (m_bFrozen)
    {
        m_pClawAnimationComponent->SetAnimation("stand");
        m_IdleTime = 0;
        m_LookingUpTime = 0;
        m_DuckingTime = 0;
    }

    if (m_State == ClawState_HoldingRope)
    {
        m_IdleTime = 0;
        m_LookingUpTime = 0;
        m_DuckingTime = 0;
    }

    //LOG("State: " + ToStr((int)m_State));

    m_LastState = m_State;
}

// Interface for subclasses
void ClawControllableComponent::VOnStartFalling()
{
    if (m_State == ClawState_JumpShooting ||
        m_State == ClawState_JumpAttacking)
    {
        return;
    }

    m_pClawAnimationComponent->SetAnimation("fall");
    m_State = ClawState_Falling;
}

void ClawControllableComponent::VOnLandOnGround()
{
    m_pClawAnimationComponent->SetAnimation("stand");
    m_State = ClawState_Standing;
}

void ClawControllableComponent::VOnStartJumping()
{
    if (m_State == ClawState_JumpShooting ||
        m_State == ClawState_JumpAttacking ||
        m_bFrozen)
    {
        return;
    }

    m_pClawAnimationComponent->SetAnimation("jump");
    m_State = ClawState_Jumping;
}

void ClawControllableComponent::VOnDirectionChange(Direction direction)
{
    if (m_bFrozen)
    {
        return;
    }

    m_pRenderComponent->SetMirrored(direction == Direction_Left);
    m_Direction = direction;
}

void ClawControllableComponent::VOnStopMoving()
{
    if (m_State == ClawState_Shooting ||
        m_State == ClawState_Idle ||
        IsDucking())
    {
        return;
    }

    m_pClawAnimationComponent->SetAnimation("stand");
    m_State = ClawState_Standing;
}

void ClawControllableComponent::VOnRun()
{
    if (m_State == ClawState_Shooting)
    {
        return;
    }
    m_pClawAnimationComponent->SetAnimation("walk");
    m_State = ClawState_Walking;
}

void ClawControllableComponent::VOnClimb()
{
    m_pClawAnimationComponent->ResumeAnimation();
    m_pClawAnimationComponent->SetAnimation("climb");
    m_State = ClawState_Climbing;
}

void ClawControllableComponent::VOnStopClimbing()
{
    m_pClawAnimationComponent->PauseAnimation();
}

void ClawControllableComponent::OnAttack()
{
    if (IsAttackingOrShooting() ||
        m_State == ClawState_Climbing ||
        m_State == ClawState_TakingDamage ||
        m_State == ClawState_Dying ||
        m_State == ClawState_HoldingRope ||
        m_bFrozen)
    {
        return;
    }

    if (m_State == ClawState_Falling ||
        m_State == ClawState_Jumping)
    {
        m_pClawAnimationComponent->SetAnimation("jumpswipe");
        m_State = ClawState_JumpAttacking;
    }
    else if (IsDucking())
    {
        m_pClawAnimationComponent->SetAnimation("duckswipe");
        m_State = ClawState_DuckAttacking;
    }
    else
    {
        // Fire/Ice/Lightning sword powerups have always swipe anim
        if (m_pPowerupComponent->HasPowerup(PowerupType_FireSword) ||
            m_pPowerupComponent->HasPowerup(PowerupType_FrostSword) ||
            m_pPowerupComponent->HasPowerup(PowerupType_LightningSword))
        {
            m_pClawAnimationComponent->SetAnimation("swipe");
        }
        else
        {
            int attackType = rand() % 5;
            if (attackType == 0)
            {
                m_pClawAnimationComponent->SetAnimation("kick");
            }
            else if (attackType == 1)
            {
                m_pClawAnimationComponent->SetAnimation("uppercut");
            }
            else
            {
                m_pClawAnimationComponent->SetAnimation("swipe");
            }
        }
        
        m_State = ClawState_Attacking;
    }

    // If its one of the magic swords, play its corresponding sound

    if (m_pPowerupComponent->HasPowerup(PowerupType_FireSword))
    {
        SoundInfo soundInfo(SOUND_CLAW_FIRE_SWORD);
        IEventMgr::Get()->VTriggerEvent(IEventDataPtr(
            new EventData_Request_Play_Sound(soundInfo)));
    }
    else if (m_pPowerupComponent->HasPowerup(PowerupType_FrostSword))
    {
        SoundInfo soundInfo(SOUND_CLAW_FROST_SWORD);
        IEventMgr::Get()->VTriggerEvent(IEventDataPtr(
            new EventData_Request_Play_Sound(soundInfo)));
    }
    else if (m_pPowerupComponent->HasPowerup(PowerupType_LightningSword))
    {
        SoundInfo soundInfo(SOUND_CLAW_LIGHTNING_SWORD);
        IEventMgr::Get()->VTriggerEvent(IEventDataPtr(
            new EventData_Request_Play_Sound(soundInfo)));
    }
}

void ClawControllableComponent::OnFire(bool outOfAmmo)
{
    if (IsAttackingOrShooting() ||
        m_State == ClawState_Climbing ||
        m_State == ClawState_Dying || 
        m_State == ClawState_TakingDamage ||
        m_State == ClawState_HoldingRope ||
        m_bFrozen)
    {
        return;
    }

    AmmoType activeAmmoType = m_pAmmoComponent->GetActiveAmmoType();
    if (m_State == ClawState_Falling ||
        m_State == ClawState_Jumping)
    {
        if (activeAmmoType == AmmoType_Pistol)
        {
            if (m_pAmmoComponent->CanFire())
            {
                m_pClawAnimationComponent->SetAnimation("jumppistol");
            }
            else
            {
                m_pClawAnimationComponent->SetAnimation("emptyjumppistol");
            }
        }
        else if (activeAmmoType == AmmoType_Magic)
        {
            if (m_pAmmoComponent->CanFire())
            {
                m_pClawAnimationComponent->SetAnimation("jumpmagic");
            }
            else
            {
                m_pClawAnimationComponent->SetAnimation("emptyjumpmagic");
            }
        }
        else if (activeAmmoType == AmmoType_Dynamite)
        {
            if (m_pAmmoComponent->CanFire())
            {
                m_pClawAnimationComponent->SetAnimation("jumpdynamite");
            }
            else
            {
                m_pClawAnimationComponent->SetAnimation("emptyjumpdynamite");
            }
        }
        m_State = ClawState_JumpShooting;
    }
    else if (IsDucking())
    {
        if (activeAmmoType == AmmoType_Pistol)
        {
            if (m_pAmmoComponent->CanFire())
            {
                m_pClawAnimationComponent->SetAnimation("duckpistol");
            }
            else
            {
                m_pClawAnimationComponent->SetAnimation("duckemptypistol");
            }
        }
        else if (activeAmmoType == AmmoType_Magic)
        {
            if (m_pAmmoComponent->CanFire())
            {
                m_pClawAnimationComponent->SetAnimation("duckmagic");
            }
            else
            {
                m_pClawAnimationComponent->SetAnimation("duckemptymagic");
            }
        }
        else if (activeAmmoType == AmmoType_Dynamite)
        {
            if (m_pAmmoComponent->CanFire())
            {
                m_pClawAnimationComponent->SetAnimation("duckpostdynamite");
            }
            else
            {
                m_pClawAnimationComponent->SetAnimation("duckemptydynamite");
            }
        }
        m_State = ClawState_DuckAttacking;
    }
    else
    {
        if (activeAmmoType == AmmoType_Pistol)
        {
            if (m_pAmmoComponent->CanFire())
            {
                m_pClawAnimationComponent->SetAnimation("pistol");
            }
            else
            {
                m_pClawAnimationComponent->SetAnimation("emptypistol");
            }
        }
        else if (activeAmmoType == AmmoType_Magic)
        {
            if (m_pAmmoComponent->CanFire())
            {
                m_pClawAnimationComponent->SetAnimation("magic");
            }
            else
            {
                m_pClawAnimationComponent->SetAnimation("emptymagic");
            }
        }
        else if (activeAmmoType == AmmoType_Dynamite)
        {
            if (m_pAmmoComponent->CanFire())
            {
                m_pClawAnimationComponent->SetAnimation("postdynamite");
            }
            else
            {
                m_pClawAnimationComponent->SetAnimation("emptydynamite");
            }
        }
        m_State = ClawState_Shooting;
    }
}

bool ClawControllableComponent::CanMove()
{
    if (m_State == ClawState_Shooting ||
        m_State == ClawState_Attacking ||
        m_State == ClawState_Dying ||
        m_State == ClawState_DuckAttacking ||
        m_State == ClawState_DuckShooting ||
        m_State == ClawState_TakingDamage ||
        m_bFrozen)
    {
        return false;
    }

    return true;
}

void ClawControllableComponent::SetCurrentPhysicsState()
{
    if (m_pPhysicsComponent->IsFalling())
    {
        m_pClawAnimationComponent->SetAnimation("fall");
        m_State = ClawState_Falling;
    }
    else if (m_pPhysicsComponent->IsJumping())
    {
        m_pClawAnimationComponent->SetAnimation("jump");
        m_State = ClawState_Jumping;
    }
    else if (IsDucking())
    {
        m_pClawAnimationComponent->SetAnimation("duck");
        m_State = ClawState_Ducking;
    }
    else if (m_pPhysicsComponent->IsOnGround())
    {
        m_pClawAnimationComponent->SetAnimation("stand");
        m_State = ClawState_Standing;
    }
    else if (m_State == ClawState_HoldingRope)
    {
        m_pClawAnimationComponent->SetAnimation("swing");
        m_pPhysicsComponent->SetGravityScale(0.0f);
        return;
    }
    else
    {
        m_pClawAnimationComponent->SetAnimation("fall");
        m_State = ClawState_Standing;
        //LOG_ERROR("Unknown physics state. Assume falling");
    }

    m_pPhysicsComponent->RestoreGravityScale();
}

void ClawControllableComponent::VOnAnimationFrameChanged(Animation* pAnimation, AnimationFrame* pLastFrame, AnimationFrame* pNewFrame)
{
    std::string animName = pAnimation->GetName();

    // Shooting, only magic and pistol supported at the moment
    if (((animName.find("pistol") != std::string::npos) && pNewFrame->hasEvent) ||
        ((animName.find("magic") != std::string::npos) && pNewFrame->hasEvent) ||
        ((animName.find("dynamite") != std::string::npos) && pNewFrame->hasEvent))
    {
        if (m_pAmmoComponent->CanFire())
        {
            AmmoType projectileType = m_pAmmoComponent->GetActiveAmmoType();
            int32 offsetX = 50;
            if (m_Direction == Direction_Left) { offsetX *= -1; }
            int32 offsetY = -20;
            if (IsDucking()) { offsetY += 40; }
            ActorTemplates::CreateClawProjectile(
                projectileType, 
                m_Direction, 
                Point(m_pPositionComponent->GetX() + offsetX, 
                m_pPositionComponent->GetY() + offsetY));

            if (IsDucking())
            {
                SetCurrentPhysicsState();
            }

            if (!g_pApp->GetGameCheats()->clawInfiniteAmmo)
            {
                // TODO: Better name of this method ? 
                m_pAmmoComponent->OnFired();
            }
        }
    }

    if (((animName.find("pistol") != std::string::npos) || 
        (animName.find("magic") != std::string::npos) || 
        (animName.find("dynamite") != std::string::npos))
        && pAnimation->IsAtLastAnimFrame())
    {
        SetCurrentPhysicsState();
    }
    else if ((animName == "swipe" ||
            animName == "kick" ||
            animName == "uppercut" ||
            animName == "jumpswipe" ||
            animName == "duckswipe"))
    {
        if (pAnimation->GetCurrentAnimationFrame()->hasEvent ||
            (animName == "swipe" && pNewFrame->idx == 3))
        {
            Point position = m_pPositionComponent->GetPosition();
            Point positionOffset(60, 20);
            if (animName == "jumpswipe")
            {
                positionOffset.y -= 10;
                positionOffset.x += 5;
            }
            if (m_Direction == Direction_Left)
            {
                positionOffset = Point(-1.0 * positionOffset.x, positionOffset.y);
            }
            position += positionOffset;

            if (m_pPowerupComponent->HasPowerup(PowerupType_FireSword))
            {
                ActorTemplates::CreateActor_Projectile(
                    ActorPrototype_FireSwordProjectile,
                    position,
                    m_Direction);
            }
            else if (m_pPowerupComponent->HasPowerup(PowerupType_FrostSword))
            {
                ActorTemplates::CreateActor_Projectile(
                    ActorPrototype_FrostSwordProjectile,
                    position,
                    m_Direction);
            }
            else if (m_pPowerupComponent->HasPowerup(PowerupType_LightningSword))
            {
                ActorTemplates::CreateActor_Projectile(
                    ActorPrototype_LightningSwordProjectile,
                    position,
                    m_Direction);
            }
            else
            {
                ActorTemplates::CreateAreaDamage(
                    position,
                    Point(50, 25),
                    10,
                    CollisionFlag_ClawAttack,
                    "Rectangle",
                    DamageType_MeleeAttack,
                    m_Direction);
            }
        }
        if (pAnimation->IsAtLastAnimFrame())
        {
            SetCurrentPhysicsState();
        }
    }
}

void ClawControllableComponent::VOnAnimationLooped(Animation* pAnimation)
{
    std::string animName = pAnimation->GetName();
    if (pAnimation->GetName().find("death") != std::string::npos)
    {
        shared_ptr<EventData_Claw_Died> pEvent(
            new EventData_Claw_Died(_owner->GetGUID(), m_pPositionComponent->GetPosition()));
        IEventMgr::Get()->VTriggerEvent(pEvent);

        SetCurrentPhysicsState();
        m_pPhysicsComponent->RestoreGravityScale();
        m_pRenderComponent->SetVisible(true);
    }
    else if (animName == "damage1" ||
             animName == "damage2")
    {
        SetCurrentPhysicsState();
    }
}

bool ClawControllableComponent::IsAttackingOrShooting()
{
    if (m_State == ClawState_Shooting ||
        m_State == ClawState_JumpShooting ||
        m_State == ClawState_DuckShooting ||
        m_State == ClawState_Attacking ||
        m_State == ClawState_DuckAttacking ||
        m_State == ClawState_JumpAttacking)
    {
        return true;
    }

    return false;
}

void ClawControllableComponent::VOnHealthBelowZero(DamageType damageType)
{
    if (m_State == ClawState_Dying)
    {
        return;
    }

    IEventMgr::Get()->VQueueEvent(IEventDataPtr(new EventData_Claw_Health_Below_Zero(_owner->GetGUID())));

    // TODO: Track how exactly claw died
    if (m_pClawAnimationComponent->GetCurrentAnimationName() != "spikedeath")
    {
        m_pClawAnimationComponent->SetAnimation("spikedeath");
        m_pPhysicsComponent->SetGravityScale(0.0f);
        m_State = ClawState_Dying;

        std::string deathSound = SOUND_CLAW_DEATH_SPIKES;

        // TODO: When claw dies, in the original game he falls off the screen and respawns
        if (damageType == DamageType_DeathTile)
        {
            int currentLevel = g_pApp->GetGameLogic()->GetCurrentLevelData()->GetLevelNumber();

            // Sound should be always like this
            deathSound = "/LEVEL" + ToStr(currentLevel) + "/SOUNDS/DEATHTILE.WAV";

            // Any special effect linked to the death and level
            switch (currentLevel)
            {
                // Spikes
                case 1:
                case 3:
                case 9:
                case 10:
                    break;
                // Liquid
                case 2:
                {
                    // Fallen into liquid - set invisible
                    m_pRenderComponent->SetVisible(false);

                    // Create tar splash
                    Point splashPosition(
                        m_pPositionComponent->GetPosition().x,
                        m_pPositionComponent->GetPosition().y - 42);
                    ActorTemplates::CreateSingleAnimation(splashPosition, AnimationType_TarSplash);
                    break;
                }
                case 4:
                {
                    // Fallen into liquid - set invisible
                    m_pRenderComponent->SetVisible(false);

                    // Create tar splash
                    Point splashPosition(
                        m_pPositionComponent->GetPosition().x,
                        m_pPositionComponent->GetPosition().y - 30);
                    ActorTemplates::CreateSingleAnimation(splashPosition, AnimationType_TarSplash);
                    break;
                }
                case 5:
                case 6:
                case 7:
                case 8:
                case 11:
                case 12:
                case 13:
                case 14:
                    assert(false && "Unsupported at the moment");
                    break;

                default:
                    assert(false && "Unknown level");
                    break;
            }
        }
        
        SoundInfo soundInfo(deathSound);
        IEventMgr::Get()->VTriggerEvent(IEventDataPtr(
            new EventData_Request_Play_Sound(soundInfo)));
    }

    if (m_bIsInBossFight)
    {
        IEventMgr::Get()->VQueueEvent(IEventDataPtr(new EventData_Boss_Fight_Ended(false)));
    }
}

void ClawControllableComponent::VOnHealthChanged(int32 oldHealth, int32 newHealth, DamageType damageType, Point impactPoint)
{
    // When claw takes damage but does not actually die
    if (newHealth > 0 && oldHealth > newHealth)
    {

        // When Claw is holding rope his animation does not change
        if (m_State != ClawState_HoldingRope)
        {
            if (Util::GetRandomNumber(0, 1) == 0)
            {
                m_pClawAnimationComponent->SetAnimation("damage1");
            }
            else
            {
                m_pClawAnimationComponent->SetAnimation("damage2");
            }
        }

        // Play random "take damage" sound
        int takeDamageSoundIdx = Util::GetRandomNumber(0, m_TakeDamageSoundList.size() - 1);
        SoundInfo soundInfo(m_TakeDamageSoundList[takeDamageSoundIdx]);
        IEventMgr::Get()->VTriggerEvent(IEventDataPtr(
            new EventData_Request_Play_Sound(soundInfo)));

        Point knockback(-10, 0);
        if (m_pRenderComponent->IsMirrored())
        {
            knockback = Point(knockback.x * -1.0, knockback.y);
        }

        // TODO: How to make this work well with enemy damage auras ?
        /*m_pPositionComponent->SetPosition(m_pPositionComponent->GetX() + knockback.x, m_pPositionComponent->GetY() + knockback.y);

        shared_ptr<EventData_Teleport_Actor> pEvent(new EventData_Teleport_Actor
            (_owner->GetGUID(), m_pPositionComponent->GetPosition()));
        IEventMgr::Get()->VQueueEvent(pEvent);*/

        m_pHealthComponent->SetInvulnerable(true);
        m_TakeDamageTimeLeftMs = m_TakeDamageDuration;
        m_pPhysicsComponent->SetGravityScale(0.0f);
        
        if (m_State != ClawState_HoldingRope)
        {
            m_State = ClawState_TakingDamage;
        }

        // Spawn graphics in the hit point
        ActorTemplates::CreateSingleAnimation(impactPoint, AnimationType_BlueHitPoint);
        Util::PlayRandomHitSound();
    }
}

bool ClawControllableComponent::IsDucking()
{
    return (m_State == ClawState_Ducking ||
        m_State == ClawState_DuckAttacking ||
        m_State == ClawState_DuckShooting);
}

void ClawControllableComponent::OnDuck()
{
    if (!IsDucking())
    {
        m_pClawAnimationComponent->SetAnimation("duck");
        m_State = ClawState_Ducking;
    }
}

void ClawControllableComponent::OnStand()
{
    m_State = ClawState_Standing;
    SetCurrentPhysicsState();
}

bool ClawControllableComponent::IsClimbing()
{
    return m_pClawAnimationComponent->GetCurrentAnimationName().find("climb") != std::string::npos &&
        !m_pClawAnimationComponent->GetCurrentAnimation()->IsPaused();
}

void ClawControllableComponent::VOnAttachedToRope()
{
    m_pClawAnimationComponent->SetAnimation("swing");
    m_pPhysicsComponent->SetGravityScale(0.0f);

    m_State = ClawState_HoldingRope;

    m_pPhysicsComponent->OnAttachedToRope();
}

void ClawControllableComponent::VDetachFromRope()
{
    m_State = ClawState_Jumping;
    m_pPhysicsComponent->RestoreGravityScale();

    SetCurrentPhysicsState();

    m_pPhysicsComponent->OnDetachedFromRope();
}

void ClawControllableComponent::BossFightStartedDelegate(IEventDataPtr pEvent)
{
    m_TakeDamageDuration = 500;

    m_bIsInBossFight = true;
}

void ClawControllableComponent::BossFightEndedDelegate(IEventDataPtr pEvent)
{
    m_TakeDamageDuration = 500;

    m_bIsInBossFight = false;
}