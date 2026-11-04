
#include "Precomp.h"
#include "UActor.h"
#include "Utils/Logger.h"
#include "VM/ScriptCall.h"
#include "VM/Frame.h"
#include "Packages/Engine/Resources/Mesh/ULodMesh.h"
#include "Engine.h"

bool UActor::HasAnim(const NameString& sequence)
{
	return Mesh() && Mesh()->GetSequence(sequence);
}

bool UActor::IsAnimating()
{
	return AnimRate() != 0.0f;
}

void UActor::FinishAnim()
{
	if (bAnimLoop())
	{
		bAnimLoop() = false;
		bAnimFinished() = false;
	}

	// The original's execFinishAnim (Engine.dll 0x103dff60): no wait with no
	// sequence, with nothing playing or tweening, or with the frame at
	// AnimLast or past; else the latent wait, which the tick releases when
	// the animation finishes (bAnimFinished): a non-loop's end, a loop's
	// AnimLast, a tween's end with no rate to go on at
	if (!StateFrame || AnimSequence().IsNone())
		return;
	bool moving = AnimFrame() < 0.0f ? TweenRate() != 0.0f : AnimRate() != 0.0f;
	if (!moving || AnimFrame() >= AnimLast())
		return;
	StateFrame->LatentState = LatentRunState::FinishAnim;
}

NameString UActor::GetAnimGroup(const NameString& sequence)
{
	if (Mesh())
	{
		MeshAnimSeq* seq = Mesh()->GetSequence(sequence);
		if (seq)
			return seq->Group;
	}
	return {};
}

// UnrealScript variables controlling animation:
// 
// Tweening means animating (using vertex interpolation) from the last animation's frame to the current animation's first frame
//
// Mesh          - the mesh the animation belongs to
// AnimSequence  - current active animation sequence
// AnimFrame     - how far we've gotten in an animation 0.0 to 1.0 for current animation, negative for interpolation from old animation when tweening
// AnimLast      - end point for AnimFrame (when to stop/loop). It is zero when only tweening (don't play the animation). It is the start of the last frame (1-1/numframes) when playing an animation
// AnimRate      - how far AnimFrame moves in 1 second (AnimFrame += AnimRate * timeElapsed). If negative it is a scale factor used to convert Velocity length to animation speed
// AnimMinRate   - the minimum animation speed when AnimRate is negative (negative AnimRate means it should use length(Velocity) * abs(AnimRate) as the anim speed)
// TweenRate     - how fast to move when AnimFrame is negative (AnimFrame += TweenRate * timeElapsed)
// OldAnimRate   - AnimRate from previous call to PlayAnim/LoopAnim/TweenAnim
// bAnimLoop     - true if the animation should loop when AnimLast is reached
// bAnimNotify   - true if animation notify events should be fired when animating
// bAnimFinished - true if AnimLast was reached and there's no looping

// The tween into a sequence, as the original's execPlayAnim (Engine.dll
// 0x103e0160) and execLoopAnim (0x103e05e0) set it after the sequence's rate
// and last frame. A one-frame sequence tweens its whole frame over the time
// given, or in 0.1 s. Else a time given tweens a frame over it; -1, the
// default, tweens a frame at the last animation's rate -- in 0.025 s a frame
// after a tween or nothing, or at the greater of the velocity's rate and half
// the new rate after a velocity-scaled one; 0, or any other, tweens nothing
// and starts the frame a shade past 0 (noTweenFrame). The caller makes the
// default 0 when nothing played before: there is nothing to tween from.
void UActor::TweenIntoAnim(const MeshAnimSeq* seq, float tweenTime, float noTweenFrame)
{
	if (AnimLast() == 0.0f)
	{
		AnimMinRate() = 0.0f;
		bAnimNotify() = false;
		OldAnimRate() = 0.0f;
		TweenRate() = tweenTime > 0.0f ? 1.0f / tweenTime : 10.0f;
		AnimRate() = 0.0f;
		AnimFrame() = -1.0f / seq->NumFrames;
	}
	else if (tweenTime > 0.0f)
	{
		TweenRate() = 1.0f / (seq->NumFrames * tweenTime);
		AnimFrame() = -1.0f / seq->NumFrames;
	}
	else if (tweenTime == -1.0f)
	{
		AnimFrame() = -1.0f / seq->NumFrames;
		if (OldAnimRate() > 0.0f)
			TweenRate() = OldAnimRate();
		else if (OldAnimRate() == 0.0f)
			TweenRate() = 1.0f / (seq->NumFrames * 0.025f);
		else
			TweenRate() = std::max(length(Velocity()) * -OldAnimRate(), AnimRate() * 0.5f);
	}
	else
	{
		TweenRate() = 0.0f;
		AnimFrame() = noTweenFrame;
	}
}

// The original's execPlayAnim (Engine.dll 0x103e0160; Rate 1 and TweenTime
// -1 by default): the sequence's rate and last frame, no loop, notifies as
// the sequence has them, the tween in (TweenIntoAnim), the animation packed
// for the net (changed if the same, so it goes again), and the rate kept as
// the last one's. No mesh or a sequence the mesh lacks logs and does nothing.
void UActor::PlayAnim(const NameString& sequence, float rate, float tweenTime)
{
	if (!Mesh())
	{
		LogMessage("PlayAnim: No mesh");
		return;
	}
	MeshAnimSeq* seq = Mesh()->GetSequence(sequence);
	if (!seq)
	{
		LogMessage("PlayAnim: Sequence '" + sequence.ToString() + "' not found in Mesh '" + Mesh()->Name.ToString() + "'");
		return;
	}

	SetTweenFromAnimFrame();
	if (AnimSequence().IsNone())
		tweenTime = 0.0f;
	AnimSequence() = sequence;
	AnimRate() = rate * seq->Rate / seq->NumFrames;
	AnimLast() = 1.0f - 1.0f / seq->NumFrames;
	bAnimFinished() = false;
	bAnimLoop() = false;
	bAnimNotify() = !seq->Notifys.empty();
	TweenIntoAnim(seq, tweenTime, 0.001f);

	vec4 old = SimAnim();
	PackSimAnim(AnimLast());
	if (SimAnim() == old)
		SimAnim().w += 1.0f;
	OldAnimRate() = AnimRate();
}

// The animation as a server sends it for a client to play (SimAnim, which
// the client unpacks; dx-reverse-info/network.md, animation): its frame, rate and
// tween rate scaled to whole numbers, and its end, negative for a loop.
void UActor::PackSimAnim(float last)
{
	SimAnim().x = AnimFrame() * 10000.0f;
	SimAnim().y = std::min(AnimRate() * 5000.0f, 32767.0f);
	SimAnim().z = TweenRate() * 1000.0f;
	SimAnim().w = last * 10000.0f;
}

// A slot's animation packed as the original's PlayBlendAnim and
// TweenBlendAnim pack it (Engine.dll 0x103e1053, 0x103e1433): the frame x
// 10,000, the rate x 10,000, the tween rate x 1,000 and the last frame x
// 10,000 as X, Y, Z and W. No game sends it: a script reads it, and a save
// keeps it.
void UActor::PackSimBlendAnim(int slot)
{
	SimBlendAnim()[slot].x = BlendAnimFrame()[slot] * 10000.0f;
	SimBlendAnim()[slot].y = BlendAnimRate()[slot] * 10000.0f;
	SimBlendAnim()[slot].z = BlendTweenRate()[slot] * 1000.0f;
	SimBlendAnim()[slot].w = BlendAnimLast()[slot] * 10000.0f;
}

// The original's execPlayBlendAnim (Engine.dll 0x103e0d80; Rate 1, TweenTime
// -1 and slot 0 by default): PlayAnim for a slot, without the main
// animation's notifies or loop. A slot with no sequence yet takes no tween
// time (0x103e0e58), so its first sequence starts untweened at 0.001, a
// one-frame one tweened in 0.1 s. Then the sequence's rate and last frame,
// and the tween in: a one-frame sequence tweens its whole frame, from -1,
// at 1 / TweenTime or 10 a second with none, its rate, least and old rates
// 0 (0x103e0eb1, 0x103e0efa) -- the head turns, one frame on every human
// mesh; else a TweenTime over 0 tweens a frame over it; -1 tweens a frame at
// OldBlendAnimRate, in 0.025 s a frame when that is 0, at the greater of the
// velocity's rate and half the new rate after a velocity-scaled one
// (0x103e0f6b); any other tweens nothing, the frame 0.001 (0x103e1040).
// SimBlendAnim packed, its W one more when all four are as they were, so
// the same animation again goes again (0x103e112d); then the rate kept as
// the slot's last.
void UActor::PlayBlendAnim(const NameString& sequenceName, float rate, float tweenTime, int blendSlot)
{
	// The original's texts; its slot message reads the mesh's name with no
	// mesh, a crash: None here
	if (blendSlot < 0 || blendSlot > 3)
	{
		LogMessage("PlayBlendAnim: BlendSlot out of range (0-3) for Mesh '" + (Mesh() ? Mesh()->Name.ToString() : std::string("None")) + "'");
		return;
	}
	if (!Mesh())
	{
		LogMessage("PlayBlendAnim: No mesh");
		return;
	}

	MeshAnimSeq* sequence = Mesh()->GetSequence(sequenceName);
	if (!sequence)
	{
		LogMessage("PlayBlendAnim: Sequence '" + sequenceName.ToString() + "' not found in Mesh '" + Mesh()->Name.ToString() + "'");
		return;
	}

	int numFrames = sequence->NumFrames;

	SetTweenFromBlendAnimFrame(blendSlot);
	if (BlendAnimSequence()[blendSlot].IsNone())
		tweenTime = 0.0f;
	BlendAnimSequence()[blendSlot] = sequenceName;
	BlendAnimRate()[blendSlot] = rate * sequence->Rate / numFrames;
	BlendAnimLast()[blendSlot] = 1.0f - 1.0f / numFrames;

	if (BlendAnimLast()[blendSlot] == 0.0f)
	{
		BlendAnimMinRate()[blendSlot] = 0.0f;
		OldBlendAnimRate()[blendSlot] = 0.0f;
		BlendTweenRate()[blendSlot] = tweenTime > 0.0f ? 1.0f / tweenTime : 10.0f;
		BlendAnimFrame()[blendSlot] = -1.0f / numFrames;
		BlendAnimRate()[blendSlot] = 0.0f;
	}
	else if (tweenTime > 0.0f)
	{
		BlendTweenRate()[blendSlot] = 1.0f / (numFrames * tweenTime);
		BlendAnimFrame()[blendSlot] = -1.0f / numFrames;
	}
	else if (tweenTime == -1.0f)
	{
		BlendAnimFrame()[blendSlot] = -1.0f / numFrames;
		float oldRate = OldBlendAnimRate()[blendSlot];
		if (oldRate > 0.0f)
			BlendTweenRate()[blendSlot] = oldRate;
		else if (oldRate == 0.0f)
			BlendTweenRate()[blendSlot] = 1.0f / (numFrames * 0.025f);
		else
			BlendTweenRate()[blendSlot] = std::max(length(Velocity()) * -oldRate, BlendAnimRate()[blendSlot] * 0.5f);
	}
	else
	{
		BlendTweenRate()[blendSlot] = 0.0f;
		BlendAnimFrame()[blendSlot] = 0.001f;
	}

	vec4 old = SimBlendAnim()[blendSlot];
	PackSimBlendAnim(blendSlot);
	if (SimBlendAnim()[blendSlot] == old)
		SimBlendAnim()[blendSlot].w += 1.0f;
	OldBlendAnimRate()[blendSlot] = BlendAnimRate()[blendSlot];
}

// The original's execTweenBlendAnim (Engine.dll 0x103e1300; slot 0 by
// default): TweenAnim for a slot, from its last pose toward the sequence's
// first frame. The sequence set with no rate, least or old rate or last
// frame (0x103e13ac); a Time over 0 tweens a frame over it, none tweens
// nothing, the frame 0; SimBlendAnim packed, never bumped (0x103e1433). A
// slot with no sequence keeps its time, unlike PlayBlendAnim's.
void UActor::TweenBlendAnim(const NameString& sequenceName, float time, int blendSlot)
{
	// The original's texts, a slot out of range PlayBlendAnim's (0x103e150d),
	// its mesh's name read with no mesh, a crash: None here
	if (blendSlot < 0 || blendSlot > 3)
	{
		LogMessage("PlayBlendAnim: BlendSlot out of range (0-3) for Mesh '" + (Mesh() ? Mesh()->Name.ToString() : std::string("None")) + "'");
		return;
	}
	if (!Mesh())
	{
		LogMessage("TweenBlendAnim: No mesh");
		return;
	}

	MeshAnimSeq* sequence = Mesh()->GetSequence(sequenceName);
	if (!sequence)
	{
		LogMessage("TweenBlendAnim: Sequence '" + sequenceName.ToString() + "' not found in Mesh '" + Mesh()->Name.ToString() + "'");
		return;
	}
	int numFrames = sequence->NumFrames;

	SetTweenFromBlendAnimFrame(blendSlot);
	BlendAnimSequence()[blendSlot] = sequenceName;
	BlendAnimLast()[blendSlot] = 0.0f;
	BlendAnimMinRate()[blendSlot] = 0.0f;
	BlendAnimRate()[blendSlot] = 0.0f;
	OldBlendAnimRate()[blendSlot] = 0.0f;
	if (time > 0.0f)
	{
		BlendTweenRate()[blendSlot] = 1.0f / (numFrames * time);
		BlendAnimFrame()[blendSlot] = -1.0f / numFrames;
	}
	else
	{
		BlendTweenRate()[blendSlot] = 0.0f;
		BlendAnimFrame()[blendSlot] = 0.0f;
	}
	PackSimBlendAnim(blendSlot);
}

// The original's execLoopAnim (Engine.dll 0x103e05e0; Rate 1, TweenTime -1
// and MinRate 0 by default). The same sequence looping and moving (tweening
// at a rate, or playing at one) goes on: its rate and least rate alone
// change, the packed animation marked changed if the same. Else the sequence
// starts as PlayAnim's does, looping, its least rate MinRate of the
// sequence's own, its tween in (TweenIntoAnim) and the animation packed
// with its last frame negative, a loop's. No mesh or a sequence the mesh
// lacks logs and does nothing.
void UActor::LoopAnim(const NameString& sequence, float rate, float tweenTime, float minRate)
{
	if (!Mesh())
	{
		LogMessage("LoopAnim: No mesh");
		return;
	}
	MeshAnimSeq* seq = Mesh()->GetSequence(sequence);
	if (!seq)
	{
		LogMessage("LoopAnim: Sequence '" + sequence.ToString() + "' not found in Mesh '" + Mesh()->Name.ToString() + "'");
		return;
	}

	float minAnimRate = minRate == 0.0f ? 0.0f : seq->Rate / seq->NumFrames * minRate;
	bool moving = AnimFrame() < 0.0f ? TweenRate() != 0.0f : AnimRate() != 0.0f;
	if (AnimSequence() == sequence && bAnimLoop() && !AnimSequence().IsNone() && moving)
	{
		bAnimFinished() = false;
		AnimRate() = rate * seq->Rate / seq->NumFrames;
		AnimMinRate() = minAnimRate;
		vec4 old = SimAnim();
		SimAnim().y = AnimRate() * 5000.0f;
		OldAnimRate() = AnimRate();
		SimAnim().w = (1.0f - 1.0f / seq->NumFrames) * -10000.0f;
		if (SimAnim() == old)
			SimAnim().w += 1.0f;
		return;
	}

	SetTweenFromAnimFrame();
	if (AnimSequence().IsNone())
		tweenTime = 0.0f;
	AnimSequence() = sequence;
	AnimRate() = rate * seq->Rate / seq->NumFrames;
	AnimLast() = 1.0f - 1.0f / seq->NumFrames;
	AnimMinRate() = minAnimRate;
	bAnimFinished() = false;
	bAnimNotify() = !seq->Notifys.empty();
	bAnimLoop() = true;
	TweenIntoAnim(seq, tweenTime, 0.0001f);
	OldAnimRate() = AnimRate();
	PackSimAnim(-AnimLast());
}

// The original's execTweenAnim (Engine.dll 0x103e0bd0; Time 0 by default): a
// tween alone to the sequence's first frame, over the time, or none with no
// time, nothing playing after it and no loop, notify or last frame. No mesh
// or a sequence the mesh lacks logs and does nothing.
void UActor::TweenAnim(const NameString& sequence, float tweenTime)
{
	if (!Mesh())
	{
		LogMessage("TweenAnim: No mesh");
		return;
	}
	MeshAnimSeq* seq = Mesh()->GetSequence(sequence);
	if (!seq)
	{
		LogMessage("TweenAnim: Sequence '" + sequence.ToString() + "' not found in Mesh '" + Mesh()->Name.ToString() + "'");
		return;
	}

	SetTweenFromAnimFrame();
	bAnimFinished() = false;
	bAnimLoop() = false;
	bAnimNotify() = false;
	AnimSequence() = sequence;
	AnimLast() = 0.0f;
	AnimMinRate() = 0.0f;
	AnimRate() = 0.0f;
	OldAnimRate() = 0.0f;
	if (tweenTime > 0.0f)
	{
		TweenRate() = 1.0f / (seq->NumFrames * tweenTime);
		AnimFrame() = -1.0f / seq->NumFrames;
	}
	else
	{
		TweenRate() = 0.0f;
		AnimFrame() = 0.0f;
	}
	PackSimAnim(0.0f);
}

void UActor::TickAnimation(float elapsed)
{
	// The original's poll (execPollFinishAnim, 0x103e00a0): the wait ends
	// once the animation has finished, a tween's included
	if (StateFrame && StateFrame->LatentState == LatentRunState::FinishAnim && bAnimFinished())
		StateFrame->LatentState = LatentRunState::Continue;

	// Four passes a tick, as the original's AActor::Tick runs them (Engine.dll
	// 0x103a1aa0; engine-dll.md, the main animation's tick): each ends at a
	// notify, a loop's wrap or the sequence's end, so a long tick through a
	// fast loop plays four loops of it and leaves the rest of its time
	// unplayed
	for (int i = 0; elapsed > 0.0f && i < 4; i++)
	{
		// If AnimFrame is positive we are doing a normal animation. If it is negative we are doing a tween animation.
		float fromAnimTime = AnimFrame();
		if (fromAnimTime >= 0.0f)
		{
			// If AnimRate is positive we are animating at a fixed rate. If it is negative we animate based on velocity (using AnimRate as a speed scale factor)
			float animRate = (AnimRate() >= 0) ? AnimRate() : std::max(AnimMinRate(), -AnimRate() * length(Velocity()));
			if (animRate == 0.0f)
				break;

			// Find what time will we be at the end of the animation
			float toAnimTime = fromAnimTime + animRate * elapsed;

			// The pass stops at the earliest notify the step crosses, the time
			// past it kept, its function called if the actor has one -- as
			// the original's, which spends the pass on a notify the actor has
			// no function for too
			if (Mesh() && bAnimNotify())
			{
				MeshAnimSeq* seq = Mesh()->GetSequence(AnimSequence());
				if (seq)
				{
					const MeshAnimNotify* first = nullptr;
					for (const MeshAnimNotify& n : seq->Notifys)
					{
						if (n.Time > fromAnimTime && n.Time <= toAnimTime && (!first || n.Time < first->Time))
							first = &n;
					}
					if (first)
					{
						elapsed = elapsed * (toAnimTime - first->Time) / (toAnimTime - fromAnimTime);
						AnimFrame() = first->Time;
						if (FindEventFunction(this, first->Function))
							CallEvent(this, first->Function);
						continue;
					}
				}
			}

			// A step short of AnimLast is the tick's whole animation
			if (toAnimTime < AnimLast())
			{
				AnimFrame() = toAnimTime;
				break;
			}

			if (bAnimLoop())
			{
				// Past AnimLast a loop goes on to 1.0 in the same pass: it
				// wraps to 0 with the time past 1.0 kept for the next pass, or
				// stops where the step took it with no time left. AnimEnd when
				// the pass crossed AnimLast from below (and a FinishAnim
				// waiting on it goes on).
				bool crossed = fromAnimTime < AnimLast();
				if (toAnimTime >= 1.0f)
				{
					elapsed = elapsed * (toAnimTime - 1.0f) / (toAnimTime - fromAnimTime);
					AnimFrame() = 0.0f;
				}
				else
				{
					AnimFrame() = toAnimTime;
					elapsed = 0.0f;
				}
				if (crossed)
				{
					if (StateFrame && StateFrame->LatentState == LatentRunState::FinishAnim)
						StateFrame->LatentState = LatentRunState::Continue;
					CallEvent(this, EventName::AnimEnd);
				}
				continue;
			}

			// A non-looping animation stops at AnimLast, its rate 0 and
			// finished, with AnimEnd; the time past it is kept, and the next
			// pass ends on the rate
			elapsed = elapsed * (toAnimTime - AnimLast()) / (toAnimTime - fromAnimTime);
			AnimFrame() = AnimLast();
			AnimRate() = 0.0f;
			bAnimFinished() = true;
			if (StateFrame && StateFrame->LatentState == LatentRunState::FinishAnim)
				StateFrame->LatentState = LatentRunState::Continue;

			CallEvent(this, EventName::AnimEnd);

			// A client that does not simulate the actor (a weapon aside) is
			// told where its animation stopped.
			if (RemoteRole() < ROLE_SimulatedProxy && !IsA("Weapon"))
			{
				SimAnim().x = AnimFrame() * 10000.0f;
				SimAnim().y = std::min(AnimRate() * 5000.0f, 32767.0f);
			}
		}
		else
		{
			float tweenRate = TweenRate();
			if (tweenRate == 0.0f)
				break;

			float toAnimTime = fromAnimTime + tweenRate * elapsed;

			float animEndTime = 0.0f;
			if (toAnimTime >= animEndTime)
			{
				elapsed -= (animEndTime - fromAnimTime) / tweenRate;
				toAnimTime = animEndTime;
			}
			else
			{
				elapsed = 0.0f;
			}

			AnimFrame() = toAnimTime;

			if (toAnimTime == animEndTime && AnimRate() == 0.0f)
			{
				if (StateFrame && StateFrame->LatentState == LatentRunState::FinishAnim)
					StateFrame->LatentState = LatentRunState::Continue;

				bAnimFinished() = true;
				//engine->LogMessage("CallEvent(AnimEnd) for " + Class->FriendlyName.ToString() + "");
				CallEvent(this, EventName::AnimEnd);
			}
		}
	}
}

void UActor::TickBlendAnimation(float elapsed)
{
	// The original moves the slots inside the main animation's tick loop
	// (dx-reverse-info/engine-dll.md, blend animations): each iteration moves
	// every slot by the frame's time, a slot that ends or finishes its
	// tween leaves the rest only the time over, and the iterations repeat
	// until spent -- three moves in a frame whose main animation had no
	// notify or end, so up to three times the slot's rate. The caller
	// gates on the main animation playing or tweening.
	for (int iteration = 0; iteration < 3 && elapsed > 0.0f; iteration++)
	{
		for (int slot = 0; slot < 4; slot++)
		{
			if (BlendAnimSequence()[slot].IsNone())
				continue;

			float frame = BlendAnimFrame()[slot];
			if (frame < 0.0f)
			{
				// A tween up from a negative frame
				float tweenRate = BlendTweenRate()[slot];
				if (tweenRate == 0.0f)
					continue;
				float to = frame + tweenRate * elapsed;
				if (to >= 0.0f)
				{
					elapsed -= (0.0f - frame) / tweenRate;
					BlendAnimFrame()[slot] = 0.0f;
				}
				else
				{
					BlendAnimFrame()[slot] = to;
				}
				continue;
			}

			float last = BlendAnimLast()[slot];
			if (frame >= last)
				continue;

			// The rate, or a velocity-scaled one
			float rate = BlendAnimRate()[slot];
			if (rate < 0.0f)
				rate = std::max(BlendAnimMinRate()[slot], -rate * length(Velocity()));
			if (rate == 0.0f)
				continue;

			float to = frame + rate * elapsed;
			if (to >= last)
			{
				// The slot stops at the last frame
				elapsed -= (last - frame) / rate;
				BlendAnimFrame()[slot] = last;
				BlendAnimRate()[slot] = 0.0f;
			}
			else
			{
				BlendAnimFrame()[slot] = to;
			}
		}
	}
}

void UActor::SetTweenFromAnimFrame()
{
	if (Mesh())
	{
		MeshAnimSeq* seq = Mesh()->GetSequence(AnimSequence());
		if (seq)
		{
			float animFrame = std::max(AnimFrame(), 0.0f) * seq->NumFrames;
			int frame0 = (int)animFrame;
			int frame1 = frame0 + 1;
			frame0 = frame0 % seq->NumFrames;
			frame1 = frame1 % seq->NumFrames;
			TweenFromAnimFrame.V0 = (seq->StartFrame + frame0) * Mesh()->FrameVerts;
			TweenFromAnimFrame.V1 = (seq->StartFrame + frame1) * Mesh()->FrameVerts;
			TweenFromAnimFrame.T = animFrame - (float)frame0;
		}
		else // For safety. Should never happen.
		{
			TweenFromAnimFrame.V0 = 0;
			TweenFromAnimFrame.V1 = 0;
			TweenFromAnimFrame.T = -1.0f;
		}
	}
}

void UActor::SetTweenFromBlendAnimFrame(int slot)
{
	if (slot < 0 || slot > 3)
		return;

	if (!Mesh())
		return;

	if (BlendAnimSequence()[slot].IsNone())
	{
		TweenFromBlendAnimFrame[slot].V0 = 0;
		TweenFromBlendAnimFrame[slot].V1 = 0;
		TweenFromBlendAnimFrame[slot].T = -1.0f;
		return;
	}

	MeshAnimSeq* seq = Mesh()->GetSequence(BlendAnimSequence()[slot]);
	if (seq)
	{
		float frame = std::max(BlendAnimFrame()[slot], 0.0f) * seq->NumFrames;
		int frame0 = (int)frame;
		int frame1 = frame0 + 1;
		frame0 = frame0 % seq->NumFrames;
		frame1 = frame1 % seq->NumFrames;
		TweenFromBlendAnimFrame[slot].V0 = (seq->StartFrame + frame0) * Mesh()->FrameVerts;
		TweenFromBlendAnimFrame[slot].V1 = (seq->StartFrame + frame1) * Mesh()->FrameVerts;
		TweenFromBlendAnimFrame[slot].T = frame - (float)frame0;
	}
	else
	{
		TweenFromBlendAnimFrame[slot].V0 = 0;
		TweenFromBlendAnimFrame[slot].V1 = 0;
		TweenFromBlendAnimFrame[slot].T = -1.0f;
	}
}

UTexture* UActor::GetMultiskin(int index)
{
	if (index >= 0 && index < 8)
		return MultiSkins()[index];
	else
		return nullptr;
}
