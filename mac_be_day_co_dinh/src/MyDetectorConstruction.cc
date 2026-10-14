// =========================================================================
// Header Definition
// =========================================================================

// 1. Include Local headers
#include "MyDetectorConstruction.hh" // Geometry definition class
#include "MySensitiveDetector.hh"    // Custom Sensitive Detector

// 2. Include Geant4 headers
#include "G4NistManager.hh"          // Material database
#include "G4Box.hh"                  // Box geometry
#include "G4Tubs.hh"                 // Cylinder geometry
#include "G4Sphere.hh"
#include "G4LogicalVolume.hh"        // Logical volume
#include "G4VPhysicalVolume.hh"
#include "G4PVPlacement.hh"          // Physical volume placement
#include "G4SubtractionSolid.hh"     // Boolean subtraction solid
#include "G4VisAttributes.hh"        // Visualization attributes
#include "G4Colour.hh"               // Colors
#include "G4Region.hh"               // Regions
#include "G4SDManager.hh"            // Sensitive detector manager
#include "G4SystemOfUnits.hh"        // Units
#include "G4RegionStore.hh"
#include "G4LogicalVolumeStore.hh"
#include "G4IStore.hh"
#include "G4RunManager.hh"           // Run manager
#include "G4OpticalSurface.hh"
#include "G4MaterialPropertiesTable.hh"
#include "G4LogicalSkinSurface.hh"
#include "G4ProductionCuts.hh"

// =========================================================================
// Geometry Dimensions
// =========================================================================
namespace GEO {
	// ===============================================
	// 1. World Volume
	// ===============================================
	const G4double WorldSize = 3 * m;

	// ===============================================
	// 2. Source
	// ===============================================
	const G4double Souce_Radius = 1 * cm;

	// ===============================================
	// 3. First Collimator
	// ===============================================
	const G4double Collimator1_InnerRadius = 2.5 * mm;
	const G4double Collimator1_Thickness = 7.5 * mm;
	const G4double Collimator1_OutterRadius = Collimator1_InnerRadius + Collimator1_Thickness;
	const G4double Collimator1_Height = 10 * cm;


	// ===============================================
	// 4. Sample
	// ===============================================
	const G4double Sample_Radius = 2.5 * cm;
	const G4double Sample_Thickness = 5 * mm;

	// ===============================================
	// 5. 2nd Collimator
	// ===============================================
	const G4double Collimator2_InnerRadius = 2.5 * mm;
	const G4double Collimator2_Thickness = 7.5 * mm;
	const G4double Collimator2_OutterRadius = Collimator2_InnerRadius + Collimator2_Thickness;
	const G4double Collimator2_Height = 10 * cm;

	// ===============================================
	// 6. NaI Detector
	// ===============================================
	const G4double Solid_NaI_Radius = 5 * cm;
	const G4double Solid_NaI_Height = 5 * cm;

	// ===============================================


}

// =========================================================================
// Constructor / Destructor
// =========================================================================
MyDetectorConstruction::MyDetectorConstruction() 
    : G4VUserDetectorConstruction(),
    fMessenger(nullptr),
	fSample_Thickness(5.0 * mm),
	fSampleMaterial("S1")
{
    fMessenger = new G4GenericMessenger(this, "/DetectorConstruction/", "Detector Construction Control");
    fMessenger->DeclareProperty("Sample_Thickness", fSample_Thickness, "Set Sample_Thickness");
    fMessenger->DeclareProperty("Sample_Material", fSampleMaterial, "Set Sample_Material");
    fMessenger->DeclareMethod("update", &MyDetectorConstruction::UpdateGeometry, "Update geometry after parameter changes");

}
MyDetectorConstruction::~MyDetectorConstruction() {}

// =========================================================================
// Define Materials
// =========================================================================
void MyDetectorConstruction::DefineMaterials() {      
    G4NistManager* NIST = G4NistManager::Instance();
    // ===============================================
	// 1. NIST Materials
    // ===============================================
    NIST->FindOrBuildMaterial("G4_AIR");
    NIST->FindOrBuildMaterial("G4_Galactic");
    NIST->FindOrBuildMaterial("G4_SODIUM_IODIDE");
	NIST->FindOrBuildMaterial("G4_Pb");

    // S0: "No sample" placeholder - vacuum
    if (!G4Material::GetMaterial("S0", false)) {
        new G4Material("S0", 1, 1.008 * g / mole, 1e-25 * g / cm3,
            kStateGas, 2.73 * kelvin, 3e-18 * pascal);
    }


    // ===============================================
	// 2. Sample Materials
    // ===============================================
    
	// Lanthanum-based oxide glass system of composition (80-x) B2O3 -10 CaO–10SiO2 - x La2O3(where x = 10, 15, 20, 25, 30 and 35 mol%)
	if (!G4Material::GetMaterial("B2O3", false)) {
		G4Material* B2O3 = new G4Material("B2O3", 2.46 * g / cm3, 2);
		B2O3->AddElement(NIST->FindOrBuildElement("B"), 2);
		B2O3->AddElement(NIST->FindOrBuildElement("O"), 3);
	}
	G4Material* B2O3 = G4Material::GetMaterial("B2O3");

	if (!G4Material::GetMaterial("CaO", false)) {
		G4Material* CaO = new G4Material("CaO", 3.34 * g / cm3, 2);
		CaO->AddElement(NIST->FindOrBuildElement("Ca"), 1);
		CaO->AddElement(NIST->FindOrBuildElement("O"), 1);
	}
	G4Material* CaO = G4Material::GetMaterial("CaO");

	if (!G4Material::GetMaterial("SiO2", false)) {
		G4Material* SiO2 = new G4Material("SiO2", 2.65 * g / cm3, 2);
		SiO2->AddElement(NIST->FindOrBuildElement("Si"), 1);
		SiO2->AddElement(NIST->FindOrBuildElement("O"), 2);
	}
	G4Material* SiO2 = G4Material::GetMaterial("SiO2");

	if (!G4Material::GetMaterial("La2O3", false)) {
		G4Material* La2O3 = new G4Material("La2O3", 6.51 * g / cm3, 2);
		La2O3->AddElement(NIST->FindOrBuildElement("La"), 2);
		La2O3->AddElement(NIST->FindOrBuildElement("O"), 3);
	}
	G4Material* La2O3 = G4Material::GetMaterial("La2O3");
	

// [1]. S1: (80-x) B2O3 -10 CaO-10SiO2 - x La2O3 with x = 10 mol%
if (!G4Material::GetMaterial("S1", false)) {
    G4Material* S1 = new G4Material("S1", 3.0099 * g / cm3, 4);
    S1->AddMaterial(B2O3, 0.5244);
    S1->AddMaterial(CaO, 0.0603);
    S1->AddMaterial(SiO2, 0.0647);
    S1->AddMaterial(La2O3, 0.3506);
}

// [2]. S2: (80-x) B2O3 -10 CaO-10SiO2 - x La2O3 with x = 15 mol%
if (!G4Material::GetMaterial("S2", false)) {
    G4Material* S2 = new G4Material("S2", 3.1999 * g / cm3, 4);
    S2->AddMaterial(B2O3, 0.4280);
    S2->AddMaterial(CaO, 0.0530);
    S2->AddMaterial(SiO2, 0.0568);
    S2->AddMaterial(La2O3, 0.4622);
}

// [3]. S3: (80-x) B2O3 -10 CaO-10SiO2 - x La2O3 with x = 20 mol%
if (!G4Material::GetMaterial("S3", false)) {
    G4Material* S3 = new G4Material("S3", 3.3899 * g / cm3, 4);
    S3->AddMaterial(B2O3, 0.3524);
    S3->AddMaterial(CaO, 0.0473);
    S3->AddMaterial(SiO2, 0.0507);
    S3->AddMaterial(La2O3, 0.5497);
}

// [4]. S4: (80-x) B2O3 -10 CaO-10SiO2 - x La2O3 with x = 25 mol%
if (!G4Material::GetMaterial("S4", false)) {
    G4Material* S4 = new G4Material("S4", 3.5799 * g / cm3, 4);
    S4->AddMaterial(B2O3, 0.2915);
    S4->AddMaterial(CaO, 0.0427);
    S4->AddMaterial(SiO2, 0.0457);
    S4->AddMaterial(La2O3, 0.6201);
}

// [5]. S5: (80-x) B2O3 -10 CaO-10SiO2 - x La2O3 with x = 30 mol%
if (!G4Material::GetMaterial("S5", false)) {
    G4Material* S5 = new G4Material("S5", 3.7699 * g / cm3, 4);
    S5->AddMaterial(B2O3, 0.2414);
    S5->AddMaterial(CaO, 0.0389);
    S5->AddMaterial(SiO2, 0.0417);
    S5->AddMaterial(La2O3, 0.6780);
}

// [6]. S6: (80-x) B2O3 -10 CaO-10SiO2 - x La2O3 with x = 35 mol%
if (!G4Material::GetMaterial("S6", false)) {
    G4Material* S6 = new G4Material("S6", 3.9599 * g / cm3, 4);
    S6->AddMaterial(B2O3, 0.1996);
    S6->AddMaterial(CaO, 0.0357);
    S6->AddMaterial(SiO2, 0.0383);
    S6->AddMaterial(La2O3, 0.7264);
}
		
}

// =========================================================================
// Construct Geometry
// =========================================================================
G4VPhysicalVolume* MyDetectorConstruction::Construct() {
    DefineMaterials();
    G4NistManager* NIST = G4NistManager::Instance();
	G4double FITNESS = 0.1 * mm; // For boolean solids, set to 1.0 for best performance

    // ===============================================
    // 1. World Volume
    // ===============================================
    G4Box* solidWorld = new G4Box("World", GEO::WorldSize / 2, GEO::WorldSize / 2, GEO::WorldSize / 2);
    G4LogicalVolume* World_LV = new G4LogicalVolume(solidWorld, NIST->FindOrBuildMaterial("G4_Galactic"), "WorldLV");
    World_Phys = new G4PVPlacement(nullptr, {}, World_LV, "World", nullptr, false, 0, true);
    
	// ===============================================
	// 2. Source
	// ===============================================
	G4Sphere* solidSource = new G4Sphere("Source", 0, GEO::Souce_Radius, 0, 360 * deg, 0, 180 * deg);
	G4LogicalVolume* Source_LV = new G4LogicalVolume(solidSource, NIST->FindOrBuildMaterial("G4_AIR"), "Source_LV");
	G4double Source_PosZ = 0;
	new G4PVPlacement(nullptr, G4ThreeVector(0, 0, Source_PosZ), Source_LV, "Source", World_LV, false, 0, true);

	// ===============================================
	// 3. Collimator 1
	// ===============================================
	G4Tubs* solidCollimator1 = new G4Tubs("Collimator1", GEO::Collimator1_InnerRadius, GEO::Collimator1_OutterRadius, GEO::Collimator1_Height / 2, 0, 360 * deg);
	G4LogicalVolume* Collimator1_LV = new G4LogicalVolume(solidCollimator1, NIST->FindOrBuildMaterial("G4_Pb"), "Collimator1_LV");
	G4double Collimator1_PosZ = Source_PosZ + GEO::Souce_Radius + GEO::Collimator1_Height / 2 + 1 * cm;
	new G4PVPlacement(nullptr, G4ThreeVector(0, 0, Collimator1_PosZ), Collimator1_LV, "Collimator1", World_LV, false, 0, true);

	// ===============================================
	// 3. Sample
	// ===============================================
	G4Tubs* solidSample = new G4Tubs("Sample", 0, GEO::Sample_Radius, fSample_Thickness / 2, 0, 360 * deg);
	G4Material* sampleMat = G4Material::GetMaterial(fSampleMaterial, false);
	if (!sampleMat) {
		sampleMat = G4Material::GetMaterial("S1");
	}
	G4LogicalVolume* Sample_LV = new G4LogicalVolume(solidSample, sampleMat, "Sample_LV");
	G4double Sample_PosZ = Collimator1_PosZ + GEO::Collimator1_Height / 2 + fSample_Thickness / 2;
	new G4PVPlacement(nullptr, G4ThreeVector(0, 0, Sample_PosZ), Sample_LV, "Sample", World_LV, false, 0, true);

	// ===============================================
	// 5. Collimator 2
	// ===============================================
	G4Tubs* solidCollimator2 = new G4Tubs("Collimator2", GEO::Collimator2_InnerRadius, GEO::Collimator2_OutterRadius, GEO::Collimator2_Height / 2, 0, 360 * deg);
	G4LogicalVolume* Collimator2_LV = new G4LogicalVolume(solidCollimator2, NIST->FindOrBuildMaterial("G4_Pb"), "Collimator2_LV");
	G4double Collimator2_PosZ = Sample_PosZ + fSample_Thickness / 2 + GEO::Collimator2_Height / 2;
	//new G4PVPlacement(nullptr, G4ThreeVector(0, 0, Collimator2_PosZ), Collimator2_LV, "Collimator2", World_LV, false, 0, true);


	// ===============================================
	// 6. NaI Crystal
	// ===============================================
	G4Tubs* solidNaI = new G4Tubs("NaI", 0, GEO::Solid_NaI_Radius, GEO::Solid_NaI_Height / 2, 0, 360 * deg);
	G4LogicalVolume* NaI_LV = new G4LogicalVolume(solidNaI, NIST->FindOrBuildMaterial("G4_SODIUM_IODIDE"), "NaI_LV");
	G4double NaI_PosZ = Collimator2_PosZ + GEO::Collimator2_Height / 2 + GEO::Solid_NaI_Height / 2;
	new G4PVPlacement(nullptr, G4ThreeVector(0, 0, NaI_PosZ), NaI_LV, "NaI", World_LV, false, 0, true);


	// ===============================================
	// VISUALIZATION
	// ===============================================

	// 1. World
	G4VisAttributes* World_VisAtt = new G4VisAttributes(G4Colour(1, 1, 1, 0.0));
	World_VisAtt->SetVisibility(false);
	World_LV->SetVisAttributes(World_VisAtt);

	// 2. Source: Red
	G4VisAttributes* Source_VisAtt = new G4VisAttributes(G4Colour(1, 0, 0));
	Source_VisAtt->SetForceSolid(true);
	Source_LV->SetVisAttributes(Source_VisAtt);

	// 3. First Collimator: Yellow
	G4VisAttributes* Collimator1_VisAtt = new G4VisAttributes(G4Colour(1, 1, 0));
	Collimator1_VisAtt->SetForceSolid(true);
	Collimator1_LV->SetVisAttributes(Collimator1_VisAtt);

	// 4. Sample: Cyan
	G4VisAttributes* Sample_VisAtt = new G4VisAttributes(G4Colour(0, 1, 1));
	Sample_VisAtt->SetForceSolid(true);
	Sample_LV->SetVisAttributes(Sample_VisAtt);

	// 5. Second Collimator: Blue
	G4VisAttributes* Collimator2_VisAtt = new G4VisAttributes(G4Colour(0, 0, 1));
	Collimator2_VisAtt->SetForceSolid(true);
	Collimator2_LV->SetVisAttributes(Collimator2_VisAtt);

	// 6. NaI Detector: Green
	G4VisAttributes* NaI_VisAtt = new G4VisAttributes(G4Colour(0, 1, 0));
	NaI_VisAtt->SetForceSolid(true);
	NaI_LV->SetVisAttributes(NaI_VisAtt);

	// ===============================================
	// ADDITIONAL SETTINGS
	// ===============================================
	
	// 1. Define a region for the Sample to set production cuts
	G4Region* Sample_Region = G4RegionStore::GetInstance()->FindOrCreateRegion("SampleRegion");
	Sample_Region->AddRootLogicalVolume(Sample_LV);
	G4ProductionCuts* sampleCuts = new G4ProductionCuts();
	sampleCuts->SetProductionCut(1 * nm, G4ProductionCuts::GetIndex("gamma"));
	sampleCuts->SetProductionCut(1 * nm, G4ProductionCuts::GetIndex("e-"));
	sampleCuts->SetProductionCut(1 * nm, G4ProductionCuts::GetIndex("e+"));
	Sample_Region->SetProductionCuts(sampleCuts);

    return World_Phys;
}

void MyDetectorConstruction::UpdateGeometry() {
    auto runManager = G4RunManager::GetRunManager();
    runManager->ReinitializeGeometry();         // rebuild geometry
    ConstructSDandField();                      // <--- reattach SDs manually
    // Print updated parameters
	G4cout << "\n================ Updated Geometry ================\n";
	G4cout << " Sample Thickness: " << fSample_Thickness / mm << " mm\n";
	G4cout << " Sample Material : " << fSampleMaterial << "\n";
}

// =========================================================================
// Construct Sensitive Detectors
// =========================================================================
void MyDetectorConstruction::ConstructSDandField() {

    G4cout << "\n================ ConstructSDandField() =================\n";

    // --- 1. Lấy SDManager và LogicalVolumeStore ---
    auto sdManager = G4SDManager::GetSDMpointer();
    auto LVS = G4LogicalVolumeStore::GetInstance();

    // --- 2. NaI Detector ---
    G4String SDname = "NaI_SD";
    auto NaI_Detector = dynamic_cast<MySensitiveDetector*>(sdManager->FindSensitiveDetector(SDname, false));
    if (!NaI_Detector) {
        NaI_Detector = new MySensitiveDetector(SDname, "NaIHitsCollection");
        sdManager->AddNewDetector(NaI_Detector);
        //G4cout << "[SD] Created new NaI_Detector" << G4endl;
    }
    else {
        //G4cout << "[SD] Reusing existing NaI_Detector" << G4endl;
    }
    // Gán SD
    for (auto lv : *LVS) {
        if (lv->GetName() == "NaI_LV") {
            lv->SetSensitiveDetector(NaI_Detector);
            //G4cout << "Attached NaI_Detector to " << lv->GetName() << G4endl;
        }
    }

    G4cout << "========================================================\n\n";
}

