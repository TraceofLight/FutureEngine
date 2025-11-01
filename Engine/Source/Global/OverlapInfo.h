#pragma once

#include "Core/Public/WeakObjectPtr.h"
#include <functional>

// Forward declarations only - do not include headers to avoid circular dependency
class UPrimitiveComponent;
class AActor;
struct FHitResult;

/**
 * Information about component overlap for tracking purposes
 * Similar to Unreal Engine's FOverlapInfo
 *
 * Uses WeakObjectPtr to safely handle deleted components.
 * Caches HitResult to avoid redundant SAT calculations.
 */
struct FOverlapInfo
{
	// The component we are overlapping with (weak reference)
	TWeakObjectPtr<UPrimitiveComponent> OverlapComponent;

	// Cached collision info (optional, only valid for blocking collisions)
	// Avoids redundant SAT calculations between Narrow Phase and Hit Event
	FHitResult* CachedHitResult = nullptr;

	FOverlapInfo()
		: OverlapComponent(nullptr)
		, CachedHitResult(nullptr)
	{
	}

	explicit FOverlapInfo(UPrimitiveComponent* InComponent, FHitResult* InHitResult = nullptr)
		: OverlapComponent(InComponent)
		, CachedHitResult(InHitResult)
	{
	}

	~FOverlapInfo()
	{
		// Clean up cached hit result
		if (CachedHitResult)
		{
			delete CachedHitResult;
			CachedHitResult = nullptr;
		}
	}

	// Copy constructor
	FOverlapInfo(const FOverlapInfo& Other)
		: OverlapComponent(Other.OverlapComponent)
		, CachedHitResult(nullptr)
	{
		if (Other.CachedHitResult)
		{
			CachedHitResult = new FHitResult(*Other.CachedHitResult);
		}
	}

	// Copy assignment
	FOverlapInfo& operator=(const FOverlapInfo& Other)
	{
		if (this != &Other)
		{
			OverlapComponent = Other.OverlapComponent;

			// Clean up old cached result
			if (CachedHitResult)
			{
				delete CachedHitResult;
				CachedHitResult = nullptr;
			}

			// Copy new cached result
			if (Other.CachedHitResult)
			{
				CachedHitResult = new FHitResult(*Other.CachedHitResult);
			}
		}
		return *this;
	}

	bool operator==(const FOverlapInfo& Other) const
	{
		return OverlapComponent == Other.OverlapComponent;
	}

	bool operator!=(const FOverlapInfo& Other) const
	{
		return !(*this == Other);
	}

	// Check if the component pointer is valid (non-null and not pending kill)
	bool IsValid() const;

	// Helper to get the owning actor of the overlapping component
	AActor* GetActor() const;
};

// Hash function for FOverlapInfo to enable std::unordered_set usage
namespace std
{
	template<>
	struct hash<FOverlapInfo>
	{
		size_t operator()(const FOverlapInfo& Info) const noexcept
		{
			// Hash based on the raw pointer value of the component
			// TWeakObjectPtr internally stores a handle, but we can use the current pointer value
			return std::hash<UPrimitiveComponent*>{}(Info.OverlapComponent.Get());
		}
	};
}
