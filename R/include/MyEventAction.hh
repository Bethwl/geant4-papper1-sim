#ifndef MYEVENTACTION_HH
#define MYEVENTACTION_HH

#include "G4UserEventAction.hh"
#include "globals.hh"
#include <set>

class G4Event;
class MyRunAction;

class MyEventAction : public G4UserEventAction {
public:
	explicit MyEventAction(MyRunAction* runAction);
	~MyEventAction() override;

	void BeginOfEventAction(const G4Event*) override;
	void EndOfEventAction(const G4Event* event) override;

	// --- NEW: first interaction tracking ---
	G4bool IsFirstInteractionRecorded() const { return fFirstInteractionRecorded; }
	void SetFirstInteractionRecorded() { fFirstInteractionRecorded = true; }

private:
	MyRunAction* fRunAction;
	std::set<G4int> fCountedNaIDet;
	G4double fEdepEvent = 0.;
	G4bool fFirstInteractionRecorded = false;
};

#endif
