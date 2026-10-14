// ============================================================================
// MyDetectorConstruction.hh
// Definition of the MyDetectorConstruction class
//
// This class is responsible for building the entire detector geometry,
// defining materials, and assigning sensitive detectors to logical volumes.
// It is registered with the G4RunManager in main.cc.
// ============================================================================

#ifndef MyDetectorConstruction_h
#define MyDetectorConstruction_h

// ============================================================================
// Geant4 Includes
// ============================================================================
#include "G4VUserDetectorConstruction.hh"
#include "G4LogicalVolume.hh"
#include "G4VPhysicalVolume.hh"
#include "G4GenericMessenger.hh"
#include "G4SystemOfUnits.hh"
#include "G4String.hh"

// ============================================================================
// MyDetectorConstruction
// ============================================================================
// Inherits from G4VUserDetectorConstruction.
//
// Responsibilities:
// - DefineMaterials(): load and register materials using G4NistManager.
// - Construct(): build geometry volumes and place them in the world.
// - ConstructSDandField(): attach sensitive detectors to logical volumes
//   (thread-safe).
// ============================================================================
class MyDetectorConstruction : public G4VUserDetectorConstruction {
public:
	// ========================================================================
	// Constructor / Destructor
	// ========================================================================
	MyDetectorConstruction();
	~MyDetectorConstruction() override;

	// ========================================================================
	// Construct(): Builds the geometry and returns the world physical volume
	// ========================================================================
	G4VPhysicalVolume* Construct() override;

	// ========================================================================
	// DefineMaterials(): Defines all materials used in the geometry
	// ========================================================================
	void DefineMaterials();

	// ========================================================================
	// ConstructSDandField(): Assigns sensitive detectors to logical volumes
	// ========================================================================
	void ConstructSDandField() override;

	G4double GetSampleDetectorDistance() const { return fSample_Thickness; }

	// NEW: getters used by MyRunAction to tag output (ROOT filename + CSV row)
	// with the material/thickness actually used for that run.
	G4String GetSampleMaterialName() const;
	G4double GetSampleThickness() const;

private:
	void UpdateGeometry();
	// Optional messenger for controlling geometry parameters at runtime
	
	G4double fSample_Thickness;
	G4String fSampleMaterial; // NEW: sample material name, set via /DetectorConstruction/Sample_Material
	G4double fCollimator2_Height; // NEW: Collimator2 height, set via /DetectorConstruction/Collimator2_Height
	
	G4VPhysicalVolume* World_Phys = nullptr;

	G4GenericMessenger* fMessenger;
};

#endif // MyDetectorConstruction_h
