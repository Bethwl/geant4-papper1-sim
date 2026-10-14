#ifndef MYRUNACTION_HH
#define MYRUNACTION_HH

#include "G4UserRunAction.hh"
#include "G4Accumulable.hh"
#include "G4Timer.hh"
#include "globals.hh"
#include "MyPrimaryGeneratorAction.hh"

class G4Run;

class MyRunAction : public G4UserRunAction {
public:
	MyRunAction();
	~MyRunAction() override;

	void BeginOfRunAction(const G4Run* run) override;
	void EndOfRunAction(const G4Run* run) override;

	void AddEdepNaIDet(G4double edep);
	void AddCountedNaIDet();

	// --- NEW ---
	void AddProcessCount(const G4String& processName);

	// --- NEW: expected-value (track-length style) estimator, xem MySteppingAction.cc ---
	void AddExpectedCount(const G4String& processName, G4double weight);

private:
	G4Accumulable<G4double> fEdepNaIDet;
	G4Accumulable<G4double> fPulseNaIDet;

	// --- NEW: process counters ---
	G4Accumulable<G4double> fNCompton;
	G4Accumulable<G4double> fNPhotoelectric;
	G4Accumulable<G4double> fNPairProduction;
	G4Accumulable<G4double> fNRayleigh;
	G4Accumulable<G4double> fNOther;
	G4Accumulable<G4double> fNTotal;

	// --- NEW: expected-value counters (track-length style, dung G4EmCalculator) ---
	G4Accumulable<G4double> fNComptonExpected;
	G4Accumulable<G4double> fNPhotoelectricExpected;
	G4Accumulable<G4double> fNPairProductionExpected;
	G4Accumulable<G4double> fNRayleighExpected;
	G4Accumulable<G4double> fNTotalExpected;

	MyPrimaryGeneratorAction* fParticleGun = nullptr;

	G4Timer fTimer;
};

#endif
