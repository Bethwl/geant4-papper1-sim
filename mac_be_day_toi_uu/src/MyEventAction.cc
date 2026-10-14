#include "MyEventAction.hh"
#include "MyRunAction.hh"
#include "MyHits.hh"

#include "G4Event.hh"
#include "G4HCofThisEvent.hh"

MyEventAction::MyEventAction(MyRunAction* runAction)
    : G4UserEventAction(), fRunAction(runAction), fEdepEvent(0.) {}

MyEventAction::~MyEventAction() = default;

void MyEventAction::BeginOfEventAction(const G4Event*) {
    fEdepEvent = 0.;
    fFirstInteractionRecorded = false;  // NEW: reset flag
}

void MyEventAction::EndOfEventAction(const G4Event* event) {
    if (!fRunAction) return;
    auto hce = event->GetHCofThisEvent();
    if (!hce) return;

    for (int i = 0; i < hce->GetNumberOfCollections(); i++) {
        auto hc = dynamic_cast<MyHitsCollection*>(hce->GetHC(i));
        if (!hc) continue;
        for (unsigned int j = 0; j < hc->entries(); j++) {
            auto hit = (*hc)[j];
            if (hit->GetVolumeName() == "NaI") {
                fEdepEvent += hit->GetEdep();
            }
        }
    }

    if (fEdepEvent > 0.) {
        fRunAction->AddEdepNaIDet(fEdepEvent);
        fRunAction->AddCountedNaIDet();
    }
}
