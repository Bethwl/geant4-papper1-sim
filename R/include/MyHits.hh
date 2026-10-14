// ============================================================================
// MyHits.hh
// Definition of the MyHit class and its collection
//
// MyHit stores information about a single hit recorded in a sensitive
// detector, such as deposited energy, particle type, and volume name.
// A collection of hits (MyHitsCollection) is created per event.
// ============================================================================

#ifndef MYHITS_HH
#define MYHITS_HH

// ============================================================================
// Geant4 Includes
// ============================================================================
#include "G4VHit.hh"
#include "G4THitsCollection.hh"
#include "G4UnitsTable.hh"
#include "G4SystemOfUnits.hh"
#include "G4ThreeVector.hh"

// C++ Includes
#include <string>

// ============================================================================
// MyHit
// ============================================================================
// Inherits from G4VHit
//
// Responsibilities:
// - Store hit information (energy deposition, weight, track ID,
//   particle name, volume name)
// - Provide setter/getter methods
// - Implement Geant4 interface (Draw, Print) for visualization/debugging
// ============================================================================
class MyHit : public G4VHit {
public:
	// ========================================================================
	// Constructor / Destructor
	// ========================================================================
	MyHit() : fEdep(0.), fWeight(0.), fTrackID(-1) {}
	~MyHit() override = default;

	// ========================================================================
	// Setters
	// ========================================================================
	void SetEdep(G4double edep) { fEdep = edep; }
	void SetWeight(G4double w) { fWeight = w; }
	void SetTrackID(G4int id) { fTrackID = id; }
	void SetParticleName(const G4String& name) { fParticleName = name; }
	void SetVolumeName(const G4String& name) { fVolumeName = name; }

	// ========================================================================
	// Getters
	// ========================================================================
	G4double GetEdep() const { return fEdep; }
	G4double GetWeight() const { return fWeight; }
	G4int GetTrackID() const { return fTrackID; }
	G4String GetParticleName() const { return fParticleName; }
	G4String GetVolumeName() const { return fVolumeName; }

	// ========================================================================
	// Geant4 interface
	// ========================================================================
	void Draw() override;   ///< Visualization hook (declared only)
	void Print() override;  ///< Debug print (declared only)

private:
	// ========================================================================
	// Data Members
	// ========================================================================
	G4double fEdep;          ///< Energy deposition in this hit
	G4double fWeight;        ///< Statistical weight of the hit
	G4int fTrackID;          ///< Track ID associated with this hit
	G4String fParticleName;  ///< Name of the particle
	G4String fVolumeName;    ///< Logical volume name where hit occurred
};

// ============================================================================
// Typedef for hits collection
// ============================================================================
typedef G4THitsCollection<MyHit> MyHitsCollection;

#endif // MYHITS_HH
