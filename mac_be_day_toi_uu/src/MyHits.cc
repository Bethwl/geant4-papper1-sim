// =========================================================================
// Header Definition
// =========================================================================

// 1. Include Local headers
#include "MyHits.hh"

// 2. Include Geant4 headers
#include "G4VVisManager.hh"
#include "G4Circle.hh"
#include "G4Colour.hh"
#include "G4VisAttributes.hh"
#include "G4ios.hh"

// ====================================================================
// DRAW METHOD IMPLEMENTATION
// ====================================================================
void MyHit::Draw()
{
    G4VVisManager* pVVisManager = G4VVisManager::GetConcreteInstance();
    if (pVVisManager)
    {
        // Nếu muốn, vẽ điểm hit, hiện tại để trống
    }
}

// ====================================================================
// PRINT METHOD IMPLEMENTATION
// ====================================================================
void MyHit::Print()
{
    G4cout
        << "  --- Hit ---"
        << "  Volume: " << fVolumeName
        << "  Particle: " << fParticleName
        << "  Edep: " << G4BestUnit(fEdep, "Energy")
        << "  Weight (VR): " << fWeight
        << G4endl;
}
