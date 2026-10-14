#ifndef MYSTEPPINGACTION_HH
#define MYSTEPPINGACTION_HH

#include "G4UserSteppingAction.hh"
#include "globals.hh"
#include "G4EmCalculator.hh"   // NEW: de tra tiet dien vi mo tung kenh (expected-value estimator)

class MyRunAction;
class MyEventAction;

class MySteppingAction : public G4UserSteppingAction {
public:
    MySteppingAction(MyRunAction* runAction, MyEventAction* eventAction);
    ~MySteppingAction() override = default;
    void UserSteppingAction(const G4Step* step) override;

private:
    MyRunAction* fRunAction;
    MyEventAction* fEventAction;

    // NEW [FIX MT]: member (KHONG static) -- moi worker thread trong Geant4 MT
    // co 1 instance MySteppingAction rieng, nen fEmCalc tu dong an toan tuyet
    // doi giua cac thread, khong bi race condition nhu "static G4EmCalculator"
    // local truoc do (nguyen nhan gay crash am tham / ROOT file 1KB).
    G4EmCalculator fEmCalc;
};

#endif
