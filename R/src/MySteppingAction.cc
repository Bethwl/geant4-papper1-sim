#include "MySteppingAction.hh"
#include "MyRunAction.hh"
#include "MyEventAction.hh"

#include "G4Step.hh"
#include "G4Track.hh"
#include "G4VProcess.hh"
#include "G4VPhysicalVolume.hh"
#include "G4EmCalculator.hh"   // NEW: de tra cuu tiet dien vi mo tung kenh, khong can transport that
#include "G4EventManager.hh"   // NEW: de ABORT event ngay sau khi xu ly xong trong Sample
#include "G4AffineTransform.hh"
#include "G4VSolid.hh"

MySteppingAction::MySteppingAction(MyRunAction* runAction, MyEventAction* eventAction)
    : fRunAction(runAction), fEventAction(eventAction)
{
}

void MySteppingAction::UserSteppingAction(const G4Step* step)
{
    if (fEventAction->IsFirstInteractionRecorded()) return;

    auto track = step->GetTrack();
    if (track->GetTrackID() != 1) return;
    if (track->GetDefinition()->GetParticleName() != "gamma") return;

    auto volume = step->GetPreStepPoint()->GetTouchableHandle()->GetVolume();
    if (!volume || volume->GetName() != "Sample") return;

    // =====================================================================
    // NEW: EXPECTED-VALUE ESTIMATOR (track-length style, giong ban chat
    // tally track-length cua OpenMC) -- chay SONG SONG voi ban analog cu
    // ben duoi, KHONG thay the, de ban co the doi chieu 2 ket qua.
    //
    // Y tuong: thay vi doi hat THAT SU tuong tac roi moi +1 (de "trang tay"
    // o vung xac suat nho), ta tinh thang xac suat tuong tac trong step nay
    // bang cong thuc Beer-Lambert cho tung kenh, dung G4EmCalculator de tra
    // tiet dien vi mo (macroscopic cross section, 1/cm) tai dung nang luong
    // hien tai cua hat -- day chinh la cong cu ban hoi luc dau (G4EmCalculator),
    // dung o day KHONG phai de thay the transport, ma de LAM MUOT phan dem.
    // =====================================================================
    {
        auto material = volume->GetLogicalVolume()->GetMaterial();
        auto particle = track->GetDefinition();
        G4double energy = step->GetPreStepPoint()->GetKineticEnergy();

        // =================================================================
        // [FIX BIAS QUAN TRONG] KHONG dung step->GetStepLength() lam t.
        //
        // step->GetStepLength() la khoang cach cua STEP HIEN TAI, va step
        // nay ket thuc vi 1 trong 2 ly do:
        //   (a) cham bien Sample (khong tuong tac)  -> t = dung do day that
        //   (b) CO tuong tac that xay ra              -> t = khoang cach
        //       NGAU NHIEN duoc lay mau den dung diem va cham do (Exp(Sigma))
        //       -- KHONG phai do day Sample!
        //
        // O nang luong thap (quang duong tu do << do day Sample), hau het
        // step roi vao truong hop (b) -> t chi la 1 mau ngau nhien ngan,
        // khong phai toan bo do day. Dat X = Sigma*t (t nay ngau nhien theo
        // Exp(Sigma)) thi X ~ Exp(1), va E[1-exp(-X)] = 1 - E[exp(-X)] =
        // 1 - 1/2 = 0.5 -- CHINH XAC giai thich vi sao N_expected bi thap
        // di dung 1 nua (he so ~2.0) o vung nang luong thap so voi analog.
        //
        // Fix: tinh t = "neu hat KHONG tuong tac thi no se di duoc bao xa
        // truoc khi ra khoi Sample" -- day la dai luong HINH HOC CO DINH,
        // khong phu thuoc viec co tuong tac that hay khong. Dung
        // G4VSolid::DistanceToOut (toa do LOCAL cua the tich Sample) -- day
        // la truy van hinh hoc thuan tuy, an toan, khong dung/anh huong
        // Navigator chinh cua qua trinh tracking.
        // =================================================================
        auto touchable = step->GetPreStepPoint()->GetTouchableHandle();
        const G4AffineTransform& transform = touchable->GetHistory()->GetTopTransform();
        G4ThreeVector localPos = transform.TransformPoint(step->GetPreStepPoint()->GetPosition());
        G4ThreeVector localDir = transform.TransformAxis(step->GetPreStepPoint()->GetMomentumDirection());
        G4double t = touchable->GetSolid()->DistanceToOut(localPos, localDir);

        // Tra tiet diet vi mo tung kenh (1/cm) tai dung nang luong hien tai.
        // NEW [FIX MT]: dung fEmCalc (member, rieng cho tung worker thread)
        // thay vi "static G4EmCalculator emCalc" (bi 35 thread dung chung ->
        // race condition -> crash am tham giua run, ROOT file dang do, khong
        // ghi duoc CSV). Moi worker thread trong Geant4 MT co instance
        // MySteppingAction rieng, nen member fEmCalc tu dong an toan tuyet doi.
        G4double sig_compt = fEmCalc.ComputeCrossSectionPerVolume(energy, particle, "compt", material);
        G4double sig_phot = fEmCalc.ComputeCrossSectionPerVolume(energy, particle, "phot", material);
        G4double sig_conv = fEmCalc.ComputeCrossSectionPerVolume(energy, particle, "conv", material);
        G4double sig_Rayl = fEmCalc.ComputeCrossSectionPerVolume(energy, particle, "Rayl", material);
        G4double sig_total = sig_compt + sig_phot + sig_conv + sig_Rayl;

        if (sig_total > 0 && t > 0) {
            // Xac suat tuong tac (bat ky kenh nao) trong step nay
            G4double P_interact = 1.0 - std::exp(-sig_total * t);

            // Chia xac suat do theo ty le tiet dien tung kenh, cong don vao
            // accumulable tuong ung -- day la "cong ky vong" thay vi "doi trung so"
            fRunAction->AddExpectedCount("compt", P_interact * sig_compt / sig_total);
            fRunAction->AddExpectedCount("phot", P_interact * sig_phot / sig_total);
            fRunAction->AddExpectedCount("conv", P_interact * sig_conv / sig_total);
            fRunAction->AddExpectedCount("Rayl", P_interact * sig_Rayl / sig_total);
        }
    }

    // =====================================================================
    // Ban ANALOG cu (giu nguyen 100%, khong doi logic) -- dem su kien THAT,
    // chi +1 dung 1 lan/event khi tuong tac that su xay ra, roi khoa lai.
    // =====================================================================
    auto process = step->GetPostStepPoint()->GetProcessDefinedStep();
    bool hasRealInteraction = false;
    if (process) {
        G4String name = process->GetProcessName();
        if (name != "Transportation" && name != "CoupledTransportation") {
            fRunAction->AddProcessCount(name);
            fEventAction->SetFirstInteractionRecorded();
            hasRealInteraction = true;
        }
    }

    // =====================================================================
    // NEW: TOI UU TOC DO + GIAM DUNG LUONG ROOT -- muc dich bay gio CHI con
    // la dem N (analog + expected), KHONG can Edep/Pulse trong NaI nua.
    // Ngay khi hat da duoc xu ly xong trong Sample (hoac co tuong tac that,
    // hoac da bay het qua Sample ma khong tuong tac), KHONG con ly do gi de
    // tiep tuc transport no (va cac hat thu cap sinh ra) qua Collimator2,
    // NaI... -- AbortCurrentEvent() huy toan bo event NGAY LAP TUC, tiet
    // kiem phan lon thoi gian CPU (NaI la vat lieu dac, sinh nhieu electron
    // thu cap, tinh scintillation...). He qua phu: AddEdepNaIDet() se KHONG
    // BAO GIO duoc goi nua (vi hat khong con toi duoc NaI) -> Ntuple "Hits"
    // va Histogram "E_dep" trong ROOT luon rong -> file ROOT tu dong nhe hon
    // han, KHONG can sua gi them o MyRunAction.cc / MySensitiveDetector.cc.
    //
    // Dieu kien abort: hoac co tuong tac that (hasRealInteraction), hoac hat
    // da roi khoi Sample ma khong tuong tac (post-step khong con o Sample).
    // =====================================================================
    auto postVolume = step->GetPostStepPoint()->GetPhysicalVolume();
    bool leftSample = (!postVolume) || (postVolume->GetName() != "Sample");

    if (hasRealInteraction || leftSample) {
        G4EventManager::GetEventManager()->AbortCurrentEvent();
    }
}