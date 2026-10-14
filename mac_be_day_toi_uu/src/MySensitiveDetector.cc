// =========================================================================
// MySensitiveDetector.cc - PHIÊN BẢN ĐÃ SỬA
// =========================================================================

#include "MySensitiveDetector.hh"
#include "MyHits.hh"

#include "G4Step.hh"
#include "G4HCofThisEvent.hh"
#include "G4SDManager.hh"
#include "G4SystemOfUnits.hh"
#include "G4VTouchable.hh"
#include "G4ios.hh"

// =========================================================================
// Constructor
// =========================================================================
MySensitiveDetector::MySensitiveDetector(const G4String& name, const G4String& collectionName)
    : G4VSensitiveDetector(name) {
    this->collectionName.push_back(collectionName);
}

// =========================================================================
// Destructor
// =========================================================================
MySensitiveDetector::~MySensitiveDetector() = default;

// =========================================================================
// Initialize
// =========================================================================
void MySensitiveDetector::Initialize(G4HCofThisEvent* hce) {
    fHitsCollection = new MyHitsCollection(SensitiveDetectorName, collectionName[0]);
    G4int hcID = G4SDManager::GetSDMpointer()->GetCollectionID(fHitsCollection);
    hce->AddHitsCollection(hcID, fHitsCollection);
}

// =========================================================================
// ProcessHits - ĐÃ ĐƠN GIẢN HÓA
// =========================================================================
// Chỉ cần ghi nhận MỌI energy deposit, không cần phân biệt particle type
// hay boundary crossing
// =========================================================================
G4bool MySensitiveDetector::ProcessHits(G4Step* aStep, G4TouchableHistory*) {

    G4double edep = aStep->GetTotalEnergyDeposit();

    // Chỉ ghi nhận nếu có năng lượng deposited
    if (edep <= 0.0) return false;

    // Tạo hit mới
    MyHit* hit = new MyHit();
    hit->SetEdep(edep);
    hit->SetVolumeName(aStep->GetPreStepPoint()->GetTouchableHandle()
        ->GetVolume()->GetName());

    // Các thông tin khác (optional, để debug)
    hit->SetWeight(aStep->GetPreStepPoint()->GetWeight());
    hit->SetParticleName(aStep->GetTrack()->GetDefinition()->GetParticleName());
    hit->SetTrackID(aStep->GetTrack()->GetTrackID());

    fHitsCollection->insert(hit);

    return true;
}