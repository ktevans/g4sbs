#include "G4SBSTargetBuilder.hh" 

#include "G4SBSDetectorConstruction.hh"
#include "G4LogicalVolume.hh"
#include "G4PVPlacement.hh"
#include "G4VisAttributes.hh"
#include "G4SubtractionSolid.hh"
#include "G4UnionSolid.hh"
#include "G4IntersectionSolid.hh"
#include "G4UserLimits.hh"
#include "G4SDManager.hh"
#include "G4Tubs.hh"
#include "G4Trd.hh"
#include "G4Cons.hh"
#include "G4Box.hh"
#include "G4Sphere.hh"
#include "G4Torus.hh"
#include "G4TwoVector.hh"
#include "G4GenericTrap.hh"
#include "G4Polycone.hh"
#include "G4ExtrudedSolid.hh"

#include "G4SBSCalSD.hh"

#include "G4SBSHArmBuilder.hh"

#include "G4SystemOfUnits.hh"
#include "G4PhysicalConstants.hh"

G4SBSTargetBuilder::G4SBSTargetBuilder(G4SBSDetectorConstruction *dc):G4SBSComponent(dc){
  assert(fDetCon);
  fTargLen = 60.0*cm;
  fTargType = G4SBS::kH2;
  fTargDen = 10.5*atmosphere/(300*kelvin*k_Boltzmann);

  fTargPos = G4ThreeVector( 0, 0, 0 );
  fTargDir = G4ThreeVector( 0, 0, 1 );

  fTargDiameter = 8.0*cm; //default of 8 cm.
  
  fFlux = false;

  fUseRad = false;
  fRadThick = 0.0;
  fRadZoffset = 10.0*cm;
  
  fSchamFlag = 0;
}

G4SBSTargetBuilder::~G4SBSTargetBuilder(){;}

void G4SBSTargetBuilder::BuildComponent(G4LogicalVolume *worldlog){
  fTargType = fDetCon->fTargType;
  //We actually need to be a little bit smarter than this: as it stands, one can never build a C foil target with this logic:
  // EFuchey 2017/02/10: organized better this with a switch instead of an endless chain of if...  else...
  //if( (fTargType == G4SBS::kLH2 || fTargType == kLD2 || fTargType == G4SBS::kCfoil ) ){
  switch(fDetCon->fExpType){
  case(G4SBS::kGEp):
  case(G4SBS::kGEPpositron):
    BuildGEpScatCham( worldlog );
  break;
  case(G4SBS::kC16):
    BuildC16ScatCham( worldlog );
    break;
  case(G4SBS::kTDIS):
    BuildTDISTarget( worldlog );
    break;
  case(G4SBS::kNDVCS):
    BuildTDISTarget( worldlog );
    break;
  case(G4SBS::kGEMHCtest):
    BuildStandardScatCham( worldlog );
    //BuildC16ScatCham( worldlog );
    break;
  case(G4SBS::kGEN):
    // BuildGasTarget( worldlog );
    BuildGEnTarget(worldlog);  
    break;
  case(G4SBS::kSIDISExp):
    BuildGasTarget( worldlog );
    break;
  default: //GMN, GEN-RP:
    BuildStandardScatCham( worldlog );
    break;
  }

  //if( fUseRad ) BuildRadiator();
  
  return;

}

// EFuchey: 2017/02/10: Making a standard function to build the cryotarget itself.
// The code for building C16 and GEp are indeed almost identical.
void G4SBSTargetBuilder::BuildStandardCryoTarget(G4LogicalVolume *motherlog, 
						 G4RotationMatrix *rot_targ, G4ThreeVector targ_offset){
  // Now let's make a cryotarget:
  //G4double Rcell = 4.0*cm;
  G4double Rcell  = fTargDiameter/2.0;
  //These are assumptions. Probably should be made user-adjustable as well.
  G4double uthick = 0.1*mm;
  G4double dthick = 0.15*mm;
  G4double sthick = 0.2*mm;
  
  G4Tubs *TargetMother_solid = new G4Tubs( "TargetMother_solid", 0, Rcell + sthick, (fTargLen+uthick+dthick)/2.0, 0.0, twopi );
  G4LogicalVolume *TargetMother_log = new G4LogicalVolume( TargetMother_solid, GetMaterial("Vacuum"), "TargetMother_log" );
  
  G4Tubs *TargetCell = new G4Tubs( "TargetCell", 0, Rcell, fTargLen/2.0, 0, twopi );
  
  G4LogicalVolume *TargetCell_log;
  
  if( fTargType == G4SBS::kLH2 ){
    TargetCell_log = new G4LogicalVolume( TargetCell, GetMaterial("LH2"), "TargetCell_log" );
  } else {
    TargetCell_log = new G4LogicalVolume( TargetCell, GetMaterial("LD2"), "TargetCell_log" );
  }

  fDetCon->InsertTargetVolume( TargetCell_log->GetName() );
  
  G4Tubs *TargetWall = new G4Tubs("TargetWall", Rcell, Rcell + sthick, fTargLen/2.0, 0, twopi );
  
  G4LogicalVolume *TargetWall_log = new G4LogicalVolume( TargetWall, GetMaterial("Al"), "TargetWall_log" );
  
  G4Tubs *UpstreamWindow = new G4Tubs("UpstreamWindow", 0, Rcell + sthick, uthick/2.0, 0, twopi );
  G4Tubs *DownstreamWindow = new G4Tubs("DownstreamWindow", 0, Rcell + sthick, dthick/2.0, 0, twopi );
  
  G4LogicalVolume *uwindow_log = new G4LogicalVolume( UpstreamWindow, GetMaterial("Al"), "uwindow_log" );
  G4LogicalVolume *dwindow_log = new G4LogicalVolume( DownstreamWindow, GetMaterial("Al"), "dwindow_log" );

  fDetCon->InsertTargetVolume( TargetWall_log->GetName() );
  fDetCon->InsertTargetVolume( uwindow_log->GetName() );
  fDetCon->InsertTargetVolume( dwindow_log->GetName() );
  
  // Now place everything:
  // Need to fix this later: Union solid defining vacuum chamber 
  // needs to be defined with the cylinder as the first solid 
  // so that we can place the target as a daughter volume at the origin!
  
  G4double ztemp = -(fTargLen+uthick+dthick)/2.0;
  // Place upstream window:
  new G4PVPlacement( 0, G4ThreeVector(0,0,ztemp+uthick/2.0), uwindow_log, "uwindow_phys", TargetMother_log, false, 0 );
  // Place target and side walls:
  ztemp += uthick;
  new G4PVPlacement( 0, G4ThreeVector(0,0,ztemp+fTargLen/2.0), TargetCell_log, "TargetCell_phys", TargetMother_log, false, 0 );
  new G4PVPlacement( 0, G4ThreeVector(0,0,ztemp+fTargLen/2.0), TargetWall_log, "TargetWall_phys", TargetMother_log, false, 0 );
  ztemp += fTargLen;
  new G4PVPlacement( 0, G4ThreeVector(0,0,ztemp+dthick/2.0), dwindow_log, "dwindow_phys", TargetMother_log, false, 0 );
  
  G4double targ_zcenter = (uthick-dthick)/2.0; //position of target center relative to target mother volume
   
  //Compute position of target relative to scattering chamber:
  //The target center should be at
  //G4RotationMatrix *rot_temp = new G4RotationMatrix;
  //rot_temp = new G4RotationMatrix();
  //rot_temp->rotateX(90.0*deg);

  //for z of target center to be at zero, 
  G4double temp = targ_offset.y();
  targ_offset.setY(temp+targ_zcenter);
  
  new G4PVPlacement( rot_targ, targ_offset, TargetMother_log, "TargetMother_phys", motherlog, false, 0 );

  if( fUseRad ){ //place radiator
    G4double yrad = fTargLen/2.0 + fRadZoffset; 
    
    G4ThreeVector radiator_pos = targ_offset - G4ThreeVector(0,yrad,0);

    BuildRadiator( motherlog, rot_targ, radiator_pos );
  }
  
  if( fFlux ){ //Make a sphere to compute particle flux:
    G4Sphere *fsph = new G4Sphere( "fsph", 1.5*fTargLen/2.0, 1.5*fTargLen/2.0+cm, 0.0*deg, 360.*deg,
				   0.*deg, 150.*deg );
    G4LogicalVolume *fsph_log = new G4LogicalVolume( fsph, GetMaterial("Air"), "fsph_log" );
    new G4PVPlacement( rot_targ, targ_offset, fsph_log, "fsph_phys", motherlog, false, 0 );
    
    G4String FluxSDname = "FLUX";
    G4String Fluxcollname = "FLUXHitsCollection";
    G4SBSCalSD *FluxSD = NULL;
    if( !( FluxSD = (G4SBSCalSD*) fDetCon->fSDman->FindSensitiveDetector(FluxSDname) ) ){
      G4cout << "Adding FLUX SD to SDman..." << G4endl;
      FluxSD = new G4SBSCalSD( FluxSDname, Fluxcollname );
      fDetCon->fSDman->AddNewDetector( FluxSD );
      (fDetCon->SDlist).insert( FluxSDname );
      fDetCon->SDtype[FluxSDname] = G4SBS::kCAL;
      
      (FluxSD->detmap).depth = 0;
    }
    fsph_log->SetSensitiveDetector( FluxSD );
    fsph_log->SetUserLimits(  new G4UserLimits(0.0, 0.0, 0.0, DBL_MAX, DBL_MAX) );
  }

  
  G4VisAttributes *Targ_visatt = new G4VisAttributes( G4Colour( 0.1, 0.05, 0.9 ) );
  TargetCell_log->SetVisAttributes( Targ_visatt );

  G4VisAttributes *TargWall_visatt = new G4VisAttributes( G4Colour( 0.9, .05, 0.1 ) );
  TargWall_visatt->SetForceWireframe( true );
  TargetWall_log->SetVisAttributes( TargWall_visatt );
  uwindow_log->SetVisAttributes( TargWall_visatt );
  dwindow_log->SetVisAttributes( TargWall_visatt );
  TargetMother_log->SetVisAttributes( G4VisAttributes::Invisible );
}

void G4SBSTargetBuilder::BuildCfoil(G4LogicalVolume *motherlog, G4RotationMatrix *rot_targ, G4ThreeVector targ_offset){
  G4double Rcell  = fTargDiameter/2.0;
  G4Tubs *Target_solid = new G4Tubs( "Target_solid", 0, Rcell, (fTargLen)/2.0, 0.0, twopi );
  G4LogicalVolume *Target_log = new G4LogicalVolume( Target_solid, GetMaterial("Carbon"), "Target_log" );
  
  G4VisAttributes* CarbonColor = new G4VisAttributes(G4Colour(0.2, 0.2, 0.2));
  Target_log->SetVisAttributes(CarbonColor);
  
  new G4PVPlacement( rot_targ, targ_offset, Target_log, "Target_phys", motherlog, false, 0 );
}

void G4SBSTargetBuilder::BuildOpticsTarget(G4LogicalVolume *motherlog, G4RotationMatrix *rot_targ, G4ThreeVector targ_offset){
  //Now the only question for the optics target is how wide we should make the foils:
  //Let's use targdiameter for now:

  char boxname[100];

  //First figure out maximum and minimum z foil positions:
  G4double zmax=-10.0*m;
  G4double zmin=+10.0*m;
  G4double maxthick = 0.0*m;
  
  for( G4int ifoil=0; ifoil<fNtargetFoils; ifoil++ ){
    if( fFoilZpos[ifoil] > zmax ) zmax = fFoilZpos[ifoil];
    if( fFoilZpos[ifoil] < zmin ) zmin = fFoilZpos[ifoil];
    if( fFoilThick[ifoil] > maxthick ) maxthick = fFoilThick[ifoil];
  }

  //  zmax += maxthick;
  //zmin -= maxthick;

  G4double zcenter = (zmax+zmin)/2.0; //midpoint between zmin and zmax: this will have to get added as an offset to the final position
  
  G4Box *MultiFoil_MotherBox = new G4Box( "MultiFoil_MotherBox", fTargDiameter/2.0+mm, fTargDiameter/2.0+mm, (zmax-zmin)/2.0+maxthick+mm );

  G4LogicalVolume *MultiFoil_MotherLog = new G4LogicalVolume( MultiFoil_MotherBox, motherlog->GetMaterial(), "MultiFoil_MotherLog" );

  MultiFoil_MotherLog->SetVisAttributes( G4VisAttributes::Invisible );
  
  G4double foilwidth = fTargDiameter/2.0;
  for( int ifoil=0; ifoil<fNtargetFoils; ifoil++ ){
    //G4String boxname = "TargFoil_box";

    sprintf(boxname, "TargFoil_box%d", ifoil );
    
    G4Box *TargFoil_box = new G4Box(G4String(boxname), fTargDiameter/2.0, fTargDiameter/2.0,  fFoilThick[ifoil]/2.0 );

    G4String logname = boxname;
    logname += "_log";
    
    G4LogicalVolume *TargFoil_log = new G4LogicalVolume( TargFoil_box, GetMaterial("Carbon"), logname );

    TargFoil_log->SetVisAttributes(new G4VisAttributes(G4Colour(0.8, 0.8, 0.8)));
    
    //    G4ThreeVector zpos(0,0,fFoilZpos[ifoil]);

    // position of foil within mother volume is z = zfoil - zcenter
    G4ThreeVector postemp(0,0,fFoilZpos[ifoil]-zcenter);

    G4String physname = boxname;
    physname += "_phys";

    new G4PVPlacement( 0, postemp, TargFoil_log, physname, MultiFoil_MotherLog, false, 0 );
    //G4ThreeVector foilpos = targ_offset + zpos;

    //Now place multi-foil target box within scattering chamber (motherlog):
    G4ThreeVector targpos = targ_offset;
    targpos += G4ThreeVector(0,0,zcenter);
    new G4PVPlacement( rot_targ, targpos, MultiFoil_MotherLog, physname, motherlog, false, 0 );

    fDetCon->InsertTargetVolume( logname );

    // G4VisAttributes *
    
  }
  
}


//This function is meant to build the "Standard" scattering chamber for GMn
void G4SBSTargetBuilder::BuildStandardScatCham(G4LogicalVolume *worldlog ){
  G4bool ChkOverlaps = false;
  G4double inch = 2.54*cm;
  
  G4LogicalVolume *logicScatChamberTank =0;
  G4LogicalVolume *logicScatChamberExitFlangePlate =0;
  G4LogicalVolume *logicScatChamberFrontClamshell =0;
  G4LogicalVolume *logicScatChamberBackClamshell =0;
  G4LogicalVolume *logicScatChamberLeftSnoutWindow =0;
  G4LogicalVolume *logicScatChamberLeftSnoutWindowFrame =0;
  G4LogicalVolume *logicScatChamberRightSnoutWindow =0;
  G4LogicalVolume *logicScatChamberRightSnoutWindowFrame =0;
  G4LogicalVolume *logicScatChamber =0;

  // Scattering chamber tank:
  // basic volume:
  G4double SCHeight = 44.75*inch;
  G4double SCRadius = 20.0*inch;
  G4double SCTankThickness = 2.5*inch;
  G4double SCTankRadius = SCRadius+SCTankThickness;
  G4double SCTankHeight = SCHeight;
  G4double SCOffset = 3.75*inch;
  
  G4Tubs* solidSCTank_0 = 
    new G4Tubs("SCTank_0", SCRadius, SCTankRadius, 0.5*SCTankHeight, 0.0*deg, 360.0*deg);
  
  // exit flange:
  G4double SCExitFlangePlateHLength = 22.5*sin(25.5*atan(1)/45.0)*inch;
  G4double SCExitFlangePlateHeight = 11.0*inch;
  G4double SCExitFlangePlateThick = 1.25*inch;
  G4double SCExitFlangeHAngleApert = atan(SCExitFlangePlateHLength/(SCTankRadius+SCExitFlangePlateThick));
  G4double SCExitFlangeMaxRad = SCExitFlangePlateHLength/sin(SCExitFlangeHAngleApert);
  
  G4Tubs* solidSCExitFlangetubs = 
    new G4Tubs("SCExFlange_tubs", SCRadius, SCExitFlangeMaxRad, 
	       0.5*SCExitFlangePlateHeight, 0.0, 2.0*SCExitFlangeHAngleApert);
  
  G4RotationMatrix* rot_temp = new G4RotationMatrix();
  rot_temp->rotateZ(180.0*deg+SCExitFlangeHAngleApert);
  
  G4UnionSolid* solidSCTank_0_exft = 
    new G4UnionSolid("solidSCTank_0_exft", solidSCTank_0, solidSCExitFlangetubs,
		     rot_temp, G4ThreeVector(0,0,SCOffset));
  
  G4Box* ExitFlangeHeadCut = new G4Box("ExitFlangeHeadCut", 0.5*m, 0.5*m, 0.5*m); 
  
  
  G4SubtractionSolid* solidSCTank_0_exf = 
    new G4SubtractionSolid("solidSCTank_0_exf", solidSCTank_0_exft, ExitFlangeHeadCut,
			   0, G4ThreeVector(-SCTankRadius-0.5*m,0,0));
  
  // exit flange hole:
  G4double SCExitFlangeHoleHeight = 7.85*inch;
  G4double SCExitFlangeHoleAngleApert = 38.25*deg;
  
  G4Tubs* solidSCExFH = 
    new G4Tubs("SCExFH", SCRadius-1.0*cm, SCTankRadius+1.5*inch,
	       0.5*SCExitFlangeHoleHeight, 0.0, SCExitFlangeHoleAngleApert);
  
  rot_temp = new G4RotationMatrix();
  rot_temp->rotateZ(180.0*deg+SCExitFlangeHoleAngleApert*0.5);
  
  G4SubtractionSolid* solidSCTank_0_exfh = 
    new G4SubtractionSolid("solidSCTank_0_exfh", solidSCTank_0_exf, solidSCExFH,
			   rot_temp, G4ThreeVector(0,0,SCOffset));
  
  // windows holes: 
  G4double SCWindowHeight = 18.0*inch;
  G4double SCWindowAngleApert = 149.0*deg;
  G4double SCWindowAngleOffset = 11.0*deg;
  
  G4Tubs* solidSCWindow = 
    new G4Tubs("SCWindow", SCRadius-1.0*cm, SCTankRadius+1.0*cm, 0.5*SCWindowHeight, 0.0, SCWindowAngleApert);
  
  rot_temp = new G4RotationMatrix();
  rot_temp->rotateZ(90.0*deg+SCWindowAngleApert*0.5-SCWindowAngleOffset);
  
  G4SubtractionSolid* solidSCTank_0_wf = 
    new G4SubtractionSolid("solidSCTank_0_wf", solidSCTank_0_exfh, solidSCWindow,
			   rot_temp, G4ThreeVector(0,0,SCOffset));
  
  rot_temp = new G4RotationMatrix();
  rot_temp->rotateZ(-90.0*deg+SCWindowAngleApert*0.5+SCWindowAngleOffset);
  
  G4SubtractionSolid* solidSCTank_0_wb = 
    new G4SubtractionSolid("solidSCTank_0_wb", solidSCTank_0_wf, solidSCWindow,
  			   rot_temp, G4ThreeVector(0,0,SCOffset));
  
  // Solid Scattering chamber tank
  G4VSolid* solidScatChamberTank = solidSCTank_0_wb;
  
  // Logic scat chamber tank
  logicScatChamberTank = 
    new G4LogicalVolume(solidScatChamberTank, GetMaterial("Aluminum"), "ScatChamberTank_log");
  
  // Scattering chamber tank placement:
  G4RotationMatrix* rotSC = new G4RotationMatrix();
  rotSC->rotateX(-90.0*deg);
  
  G4ThreeVector* SCPlacement = new G4ThreeVector(0,0,-SCOffset);
  SCPlacement->rotateX(90*deg);
  
  new G4PVPlacement(rotSC, *SCPlacement, logicScatChamberTank, "ScatChamberTankPhys", worldlog, false, 0, ChkOverlaps);
  
  //Exit Flange Plate
  G4Box* solidSCExitFlangePlate = 
    new G4Box("SCExitFlangePlate_sol", SCExitFlangePlateThick/2.0, 
	      SCExitFlangePlateHeight/2.0, SCExitFlangePlateHLength); 
  
  logicScatChamberExitFlangePlate = 
    new G4LogicalVolume(solidSCExitFlangePlate, GetMaterial("Aluminum"), "SCExitFlangePlate_log");

  new G4PVPlacement(0, G4ThreeVector(-SCTankRadius-SCExitFlangePlateThick/2.0,0,0), 
		    logicScatChamberExitFlangePlate, "SCExitFlangePlate", worldlog, false, 0, ChkOverlaps); 
  
  // Front and back Clamshells...
  // Basic solid: 
  G4double SCClamHeight = 20.0*inch;
  G4double SCClamThick = 1.25*inch;
  G4double SCClamAngleApert = 151.0*deg;
  
  G4Tubs* solidSCClamshell_0 = 
    new G4Tubs("solidSCClamshell_0", SCTankRadius, SCTankRadius+SCClamThick, 
	       0.5*SCClamHeight, 0.0, SCClamAngleApert);
  
  // Front Clamshell:
  G4double SCFrontClamOuterRadius = SCTankRadius+SCClamThick;
  G4double SCBeamExitAngleOffset = 64.5*deg;
  G4double SCLeftSnoutAngle = -24.2*deg;
  G4double SCLeftSnoutAngleOffset = SCBeamExitAngleOffset+SCLeftSnoutAngle;
  //G4double SCRightSnoutAngle = 50.1*deg;
  G4double SCRightSnoutAngle = 47.5*deg;
  G4double SCRightSnoutAngleOffset = SCBeamExitAngleOffset+SCRightSnoutAngle;
  
  // Snouts: NB: similarly to the GEp scattering chamber, 
  // the "left" and "right" is defined as viewed from downstream.
  // In other words, the "left" snout is actually on the right side of the beam 
  // (looking on the right direction) and vice-versa.
  
  // Right snout opening:
  G4double SCRightSnoutDepth = 15.0*inch;// x
  G4double SCRightSnoutWidth = 26.0*inch;// y
  G4double SCRightSnoutHeight = 18.0*inch; //z
  G4double SCRightSnoutHoleHeight = 14.0*inch;
  G4double SCRightSnoutHoleAngleApert = 55.0*deg;
  G4double SCRightSnoutBoxAngleOffset = 9.40*deg;
  
  // Basic "box"
  G4Box* RightSnoutBox = 
    new G4Box("RightSnoutBox", SCRightSnoutDepth*0.5, SCRightSnoutWidth*0.5, SCRightSnoutHeight*0.5);
  
  rot_temp = new G4RotationMatrix();
  rot_temp->rotateZ(180.0*deg-SCRightSnoutAngleOffset-SCRightSnoutBoxAngleOffset);
  
  G4UnionSolid* solidSCFrontClam_0_rsb = 
    new G4UnionSolid("solidSCClamshell_0_rsb", solidSCClamshell_0, 
		     RightSnoutBox, rot_temp, 
		     G4ThreeVector(SCFrontClamOuterRadius*sin(90.0*deg-SCRightSnoutAngleOffset),
		      		   SCFrontClamOuterRadius*cos(90.0*deg-SCRightSnoutAngleOffset), 
		     		   0)
		     );
  
  // Basic box cut: remove the surplus outside of the clamshell
  // NB: the surplus inside is removed along with the one of the left snout
  G4Box* RightSnoutBox_cut = 
    new G4Box("RightSnoutBox_cut", 
	      SCRightSnoutDepth+0.01*inch, SCRightSnoutWidth, SCRightSnoutHeight*0.5+0.01*inch);
  
  rot_temp = new G4RotationMatrix();
  rot_temp->rotateZ(180.0*deg-SCRightSnoutAngleOffset);
  
  G4SubtractionSolid* solidSCFrontClam_0_rsbc = 
    new G4SubtractionSolid("solidSCFrontClam_0_rsbc", solidSCFrontClam_0_rsb,
			   RightSnoutBox_cut, rot_temp, 
			   G4ThreeVector((SCFrontClamOuterRadius+SCRightSnoutDepth)
					 *sin(90.0*deg-SCRightSnoutAngleOffset), 
					 (SCFrontClamOuterRadius+SCRightSnoutDepth)
					 *cos(90.0*deg-SCRightSnoutAngleOffset),
					 0)
			   );
  
  // Cutting the hole...
  G4Tubs* RightSnoutApertCut = 
    new G4Tubs("RightSnoutApertCut", 0.0, 30.0*inch, SCRightSnoutHoleHeight/2.0, 
	       0.0, SCRightSnoutHoleAngleApert);
  
  rot_temp = new G4RotationMatrix();
  rot_temp->rotateZ(-SCRightSnoutAngleOffset+SCRightSnoutHoleAngleApert*0.5);
  
  G4SubtractionSolid* solidSCFrontClam_0_rs =
    new G4SubtractionSolid("solidSCClamshell_0_rs", solidSCFrontClam_0_rsbc, 
			   RightSnoutApertCut, rot_temp, G4ThreeVector(0, 0, 0));
  
  
  // Right snout window+frame:
  G4double SCRightSnoutWindowWidth = 26.351*inch;
  G4double SCSnoutWindowThick = 0.02*inch;
  G4double SCSnoutWindowFrameThick = 0.75*inch;
  
  G4double SCRightSnoutWindowDist = 23.74*inch+SCSnoutWindowThick*0.5;
  //G4double SCRightSnoutHoleWidth = 21.855*inch;
  G4double SCRightSnoutHoleWidth = 24.47*inch;
  G4double SCRightSnoutHoleCurvRad = 2.1*inch;
  G4double SCRightSnoutWindowFrameDist = SCRightSnoutWindowDist+SCSnoutWindowThick*0.5+SCSnoutWindowFrameThick*0.5;
  
  // window
  G4Box* solidRightSnoutWindow = 
    new G4Box("RightSnoutWindow_sol", SCRightSnoutWindowWidth*0.5, 
	      SCRightSnoutHeight*0.5, SCSnoutWindowThick*0.5);

  logicScatChamberRightSnoutWindow = 
    new G4LogicalVolume(solidRightSnoutWindow, GetMaterial("Aluminum"), "SCRightSnoutWindow_log");
  
  rot_temp = new G4RotationMatrix();
  rot_temp->rotateY(-SCRightSnoutAngle);
  
  new G4PVPlacement(rot_temp, 
  		    G4ThreeVector(SCRightSnoutWindowDist*sin(SCRightSnoutAngle),
  				  0,
  				  SCRightSnoutWindowDist*cos(SCRightSnoutAngle)), 
  		    logicScatChamberRightSnoutWindow, "SCRightSnoutWindow", worldlog, false, 0, ChkOverlaps);
  
  // basic window frame
  G4Box* solidRightSnoutWindowFrame_0 = 
    new G4Box("RightSnoutWindowFrame_0", SCRightSnoutWidth*0.5, 
	      SCRightSnoutHeight*0.5, SCSnoutWindowFrameThick*0.5);
  
  // + lots of cut out solids...
  G4Tubs* solidRightSnoutWindowFrame_cut_0 = 
    new G4Tubs("RightSnoutWindowFrame_cut_0", 0.0, SCRightSnoutHoleCurvRad, SCSnoutWindowFrameThick, 0.0, 360.0*deg);
  
  G4SubtractionSolid* solidRightSnoutWindowFrame_0_htr = 
    new G4SubtractionSolid("solidRightSnoutWindowFrame_0_htr", solidRightSnoutWindowFrame_0,
			   solidRightSnoutWindowFrame_cut_0, 0, 
			   G4ThreeVector(SCRightSnoutHoleWidth*0.5-SCRightSnoutHoleCurvRad, 
					 SCRightSnoutHoleHeight*0.5-SCRightSnoutHoleCurvRad,
					 0)
			   );

  G4SubtractionSolid* solidRightSnoutWindowFrame_0_hbr = 
    new G4SubtractionSolid("solidRightSnoutWindowFrame_0_hbr", solidRightSnoutWindowFrame_0_htr,
			   solidRightSnoutWindowFrame_cut_0, 0, 
			   G4ThreeVector(SCRightSnoutHoleWidth*0.5-SCRightSnoutHoleCurvRad, 
					 -SCRightSnoutHoleHeight*0.5+SCRightSnoutHoleCurvRad,
					 0)
			   );
  
  G4SubtractionSolid* solidRightSnoutWindowFrame_0_hbl = 
    new G4SubtractionSolid("solidRightSnoutWindowFrame_0_hbl", solidRightSnoutWindowFrame_0_hbr,
			   solidRightSnoutWindowFrame_cut_0, 0, 
			   G4ThreeVector(-SCRightSnoutHoleWidth*0.5+SCRightSnoutHoleCurvRad, 
					 -SCRightSnoutHoleHeight*0.5+SCRightSnoutHoleCurvRad,
					 0)
			   );
  
  G4SubtractionSolid* solidRightSnoutWindowFrame_0_htl = 
    new G4SubtractionSolid("solidRightSnoutWindowFrame_0_htl", solidRightSnoutWindowFrame_0_hbl,
			   solidRightSnoutWindowFrame_cut_0, 0, 
			   G4ThreeVector(-SCRightSnoutHoleWidth*0.5+SCRightSnoutHoleCurvRad, 
					 SCRightSnoutHoleHeight*0.5-SCRightSnoutHoleCurvRad,
					 0)
			   );
  
  G4Box* solidRightSnoutWindowFrame_cut_1 = 
    new G4Box("RightSnoutWindowFrame_cut_1", SCRightSnoutHoleWidth*0.5-SCRightSnoutHoleCurvRad, 
	      SCRightSnoutHoleHeight*0.5, SCSnoutWindowFrameThick);
  
  G4SubtractionSolid* solidRightSnoutWindowFrame_1 = 
    new G4SubtractionSolid("solidRightSnoutWindowFrame", solidRightSnoutWindowFrame_0_htl,
  			   solidRightSnoutWindowFrame_cut_1, 0, G4ThreeVector());
  
  G4Box* solidRightSnoutWindowFrame_cut_2 = 
    new G4Box("RightSnoutWindowFrame_cut_2", SCRightSnoutHoleWidth*0.5,
	      SCRightSnoutHoleHeight*0.5-SCRightSnoutHoleCurvRad, SCSnoutWindowFrameThick);
  
  G4SubtractionSolid* solidRightSnoutWindowFrame = 
    new G4SubtractionSolid("solidRightSnoutWindowFrame", solidRightSnoutWindowFrame_1,
  			   solidRightSnoutWindowFrame_cut_2, 0, G4ThreeVector());

  logicScatChamberRightSnoutWindowFrame = 
    new G4LogicalVolume(solidRightSnoutWindowFrame, GetMaterial("Aluminum"), "SCRightSnoutWindowFrame_log");
  
  rot_temp = new G4RotationMatrix();
  rot_temp->rotateY(-SCRightSnoutAngle);
  
  new G4PVPlacement(rot_temp, 
  		    G4ThreeVector(SCRightSnoutWindowFrameDist*sin(SCRightSnoutAngle),
  				  0,
  				  SCRightSnoutWindowFrameDist*cos(SCRightSnoutAngle)), 
  		    logicScatChamberRightSnoutWindowFrame, "SCRightSnoutWindowFrame", worldlog, false, 0, ChkOverlaps);
  
		    
  // Left snout opening:
  G4double SCLeftSnoutDepth = 4.0*inch;// x
  G4double SCLeftSnoutWidth = 16.338*inch;// y
  G4double SCLeftSnoutHeight = 11.0*inch; //z
  G4double SCLeftSnoutHoleHeight = 7.0*inch;
  G4double SCLeftSnoutHoleAngleApert = 30.0*deg;
  G4double SCLeftSnoutYOffset = 0.50*inch;
  
  // Basic box
  G4Box* LeftSnoutBox = 
    new G4Box("LeftSnoutBox", SCLeftSnoutDepth*0.5, SCLeftSnoutWidth*0.5, SCLeftSnoutHeight*0.5);

  rot_temp = new G4RotationMatrix();
  rot_temp->rotateZ(180.0*deg-SCLeftSnoutAngleOffset);
  
  G4UnionSolid* solidSCFrontClam_0_lsb = 
    new G4UnionSolid("solidSCClamshell_0_lsb", solidSCFrontClam_0_rs, 
		     LeftSnoutBox, rot_temp, 
		     G4ThreeVector((SCFrontClamOuterRadius-1.85*inch)*sin(90.0*deg-SCLeftSnoutAngleOffset),
		     		   (SCFrontClamOuterRadius-1.85*inch)*cos(90.0*deg-SCLeftSnoutAngleOffset), 
		     		   SCLeftSnoutYOffset)
		     );
  
  // remove all surplus material inside the scat chamber
  G4Tubs* SnoutsInnerCut = 
    new G4Tubs("SnoutsInnerCut", 0.0, SCTankRadius, SCClamHeight/2.0, 0.0, 360.0*deg);
  
  G4SubtractionSolid* solidSCFrontClam_0_lsbc =
    new G4SubtractionSolid("solidSCClamshell_0_lsbc", solidSCFrontClam_0_lsb, 
			   SnoutsInnerCut, 0, G4ThreeVector());
  
  // Cut the hole
  G4Tubs* LeftSnoutApertCut = 
    new G4Tubs("LeftSnoutApertCut", 0.0, 30.0*inch, SCLeftSnoutHoleHeight/2.0, 
	       0.0, SCLeftSnoutHoleAngleApert);
  
  rot_temp = new G4RotationMatrix();
  rot_temp->rotateZ(-SCLeftSnoutAngleOffset+SCLeftSnoutHoleAngleApert*0.5);
 
  G4SubtractionSolid* solidSCFrontClam_0_ls =
    new G4SubtractionSolid("solidSCClamshell_0_ls", solidSCFrontClam_0_lsbc, 
			   LeftSnoutApertCut, rot_temp, G4ThreeVector(0, 0, SCLeftSnoutYOffset));
  
  // Left snout window+frame:
  G4double SCLeftSnoutWindowDist = 23.9*inch+SCSnoutWindowThick*0.5;
  G4double SCLeftSnoutWindowFrameDist = SCLeftSnoutWindowDist+SCSnoutWindowThick*0.5+SCSnoutWindowFrameThick*0.5;
  G4double SCLeftSnoutHoleWidth = 12.673*inch;
  G4double SCLeftSnoutHoleCurvRad = 1.05*inch;
  
  // window
  G4Box* solidLeftSnoutWindow_0 = 
    new G4Box("LeftSnoutWindow_sol", SCLeftSnoutWidth*0.5, SCLeftSnoutHeight*0.5, SCSnoutWindowThick*0.5);
  
  G4Tubs* solidLeftSnoutWindow_cut_0 = 
    new G4Tubs("LeftSnoutWindow_cut_0", 0.0, 2.579*inch, SCSnoutWindowFrameThick, 0.0, 360.0*deg);
  
  G4SubtractionSolid* solidLeftSnoutWindow = 
    new G4SubtractionSolid("solidLeftSnoutWindow", solidLeftSnoutWindow_0, solidLeftSnoutWindow_cut_0,
			   0, G4ThreeVector(10.287*inch, -SCLeftSnoutYOffset, 0));
  
  logicScatChamberLeftSnoutWindow = 
    new G4LogicalVolume(solidLeftSnoutWindow, GetMaterial("Aluminum"), "SCLeftSnoutWindow_log");
  
  rot_temp = new G4RotationMatrix();
  rot_temp->rotateY(-SCLeftSnoutAngle);
  
  new G4PVPlacement(rot_temp, 
		    G4ThreeVector(SCLeftSnoutWindowDist*sin(SCLeftSnoutAngle),
				  SCLeftSnoutYOffset,
				  SCLeftSnoutWindowDist*cos(SCLeftSnoutAngle)), 
		    logicScatChamberLeftSnoutWindow, "SCLeftSnoutWindow", worldlog, false, 0, ChkOverlaps);
  
  // window frame
  G4Box* solidLeftSnoutWindowFrame_0 = 
    new G4Box("LeftSnoutWindowFrame_0", SCLeftSnoutWidth*0.5, 
	      SCLeftSnoutHeight*0.5, SCSnoutWindowFrameThick*0.5);
  
  // + lots of cut out solids...
  G4Tubs* solidLeftSnoutWindowFrame_cut_0 = 
    new G4Tubs("LeftSnoutWindowFrame_cut_0", 0.0, SCLeftSnoutHoleCurvRad, SCSnoutWindowFrameThick, 0.0, 360.0*deg);
  
  G4SubtractionSolid* solidLeftSnoutWindowFrame_0_htr = 
    new G4SubtractionSolid("solidLeftSnoutWindowFrame_0_htr", solidLeftSnoutWindowFrame_0,
			   solidLeftSnoutWindowFrame_cut_0, 0, 
			   G4ThreeVector(SCLeftSnoutHoleWidth*0.5-SCLeftSnoutHoleCurvRad, 
					 SCLeftSnoutHoleHeight*0.5-SCLeftSnoutHoleCurvRad,
					 0)
			   );

  G4SubtractionSolid* solidLeftSnoutWindowFrame_0_hbr = 
    new G4SubtractionSolid("solidLeftSnoutWindowFrame_0_hbr", solidLeftSnoutWindowFrame_0_htr,
			   solidLeftSnoutWindowFrame_cut_0, 0, 
			   G4ThreeVector(SCLeftSnoutHoleWidth*0.5-SCLeftSnoutHoleCurvRad, 
					 -SCLeftSnoutHoleHeight*0.5+SCLeftSnoutHoleCurvRad,
					 0)
			   );
  
  G4SubtractionSolid* solidLeftSnoutWindowFrame_0_hbl = 
    new G4SubtractionSolid("solidLeftSnoutWindowFrame_0_hbl", solidLeftSnoutWindowFrame_0_hbr,
			   solidLeftSnoutWindowFrame_cut_0, 0, 
			   G4ThreeVector(-SCLeftSnoutHoleWidth*0.5+SCLeftSnoutHoleCurvRad, 
					 -SCLeftSnoutHoleHeight*0.5+SCLeftSnoutHoleCurvRad,
					 0)
			   );
  
  G4SubtractionSolid* solidLeftSnoutWindowFrame_0_htl = 
    new G4SubtractionSolid("solidLeftSnoutWindowFrame_0_htl", solidLeftSnoutWindowFrame_0_hbl,
			   solidLeftSnoutWindowFrame_cut_0, 0, 
			   G4ThreeVector(-SCLeftSnoutHoleWidth*0.5+SCLeftSnoutHoleCurvRad, 
					 SCLeftSnoutHoleHeight*0.5-SCLeftSnoutHoleCurvRad,
					 0)
			   );
  
  G4Box* solidLeftSnoutWindowFrame_cut_1 = 
    new G4Box("LeftSnoutWindowFrame_cut_1", SCLeftSnoutHoleWidth*0.5-SCLeftSnoutHoleCurvRad, 
	      SCLeftSnoutHoleHeight*0.5, SCSnoutWindowFrameThick);
  
  G4SubtractionSolid* solidLeftSnoutWindowFrame_1 = 
    new G4SubtractionSolid("solidLeftSnoutWindowFrame", solidLeftSnoutWindowFrame_0_htl,
  			   solidLeftSnoutWindowFrame_cut_1, 0, G4ThreeVector());
  
  G4Box* solidLeftSnoutWindowFrame_cut_2 = 
    new G4Box("LeftSnoutWindowFrame_cut_2", SCLeftSnoutHoleWidth*0.5,
	      SCLeftSnoutHoleHeight*0.5-SCLeftSnoutHoleCurvRad, SCSnoutWindowFrameThick);
  
  G4SubtractionSolid* solidLeftSnoutWindowFrame_2 = 
    new G4SubtractionSolid("solidLeftSnoutWindowFrame_2", solidLeftSnoutWindowFrame_1,
  			   solidLeftSnoutWindowFrame_cut_2, 0, G4ThreeVector());

  G4SubtractionSolid* solidLeftSnoutWindowFrame = 
    new G4SubtractionSolid("solidLeftSnoutWindowFrame", solidLeftSnoutWindowFrame_2, solidLeftSnoutWindow_cut_0,
			   0, G4ThreeVector(10.287*inch, -SCLeftSnoutYOffset, 0));
  
  logicScatChamberLeftSnoutWindowFrame = 
    new G4LogicalVolume(solidLeftSnoutWindowFrame, GetMaterial("Aluminum"), "SCLeftSnoutWindowFrame_log");
  
  rot_temp = new G4RotationMatrix();
  rot_temp->rotateY(-SCLeftSnoutAngle);
  
  new G4PVPlacement(rot_temp, 
		    G4ThreeVector(SCLeftSnoutWindowFrameDist*sin(SCLeftSnoutAngle),
				  SCLeftSnoutYOffset,
				  SCLeftSnoutWindowFrameDist*cos(SCLeftSnoutAngle)), 
		    logicScatChamberLeftSnoutWindowFrame, "SCLeftSnoutWindowFrame", worldlog, false, 0, ChkOverlaps);
  
  // Exit Beam Pipe:
  // Should come after left snout
  G4Tubs* solidExitBeamPipe = new G4Tubs("solidExitBeamPipe", 
					 0.0, 55.0*mm, 2.150*inch, 0.0, 360.0*deg);
  
  rot_temp = new G4RotationMatrix();
  rot_temp->rotateX(90.0*deg);
  rot_temp->rotateY(-90.0*deg+SCBeamExitAngleOffset);
  
  G4UnionSolid* solidSCFrontClam_0_ebp =
    new G4UnionSolid("solidSCClamshell_0_ebp", solidSCFrontClam_0_ls, 
		     solidExitBeamPipe, rot_temp, 
		     G4ThreeVector((SCFrontClamOuterRadius+2.003*inch)*sin(90.0*deg-SCBeamExitAngleOffset),
				   (SCFrontClamOuterRadius+2.003*inch)*cos(90.0*deg-SCBeamExitAngleOffset), 
				   0.0)
		     );
  
  // remove extra material from the left snout around the chamber
  G4Tubs* solidExitBeamPipeSurroundCut = new G4Tubs("solidExitBeamPipeSurroundCut", 
						    55.0*mm, 2.803*inch, 1.684*inch, 0.0, 360.0*deg);
  
  G4SubtractionSolid* solidSCFrontClam_0_ebps =
    new G4SubtractionSolid("solidSCClamshell_0_ebps", solidSCFrontClam_0_ebp, 
			   solidExitBeamPipeSurroundCut, rot_temp, 
			   G4ThreeVector((SCFrontClamOuterRadius+1.684*inch)*sin(90.0*deg-SCBeamExitAngleOffset),
					 (SCFrontClamOuterRadius+1.684*inch)*cos(90.0*deg-SCBeamExitAngleOffset), 
					 0.0)
			   );
   
  G4Tubs* solidExitBeamPipeFlange = new G4Tubs("solidExitBeamPipeFlange", 
					       0.0, 2.985*inch, 0.3925*inch, 0.0, 360.0*deg);
  
  G4UnionSolid* solidSCFrontClam_0_ebpf =
    new G4UnionSolid("solidSCClamshell_0_ebpf", solidSCFrontClam_0_ebps, 
		     solidExitBeamPipeFlange, rot_temp, 
		     G4ThreeVector((SCFrontClamOuterRadius+3.7605*inch)*sin(90.0*deg-SCBeamExitAngleOffset), 
				   (SCFrontClamOuterRadius+3.7605*inch)*cos(90.0*deg-SCBeamExitAngleOffset), 
				   0.0));
  
  G4Tubs* solidExitBeamPipeHole = new G4Tubs("solidBackViewPipeHole", 
					     0.0, 50.0*mm, 7.903*inch, 0.0, 360.0*deg);
  
  G4SubtractionSolid* solidSCFrontClam_0_ebph =
    new G4SubtractionSolid("solidSCClamshell_0_ebph", solidSCFrontClam_0_ebpf, 
			   solidExitBeamPipeHole, rot_temp, 
			   G4ThreeVector(SCFrontClamOuterRadius*sin(90.0*deg-SCBeamExitAngleOffset),
					 SCFrontClamOuterRadius*cos(90.0*deg-SCBeamExitAngleOffset), 
					 0.0)
			   );
  
  // Placing Front ClamShell (at last)
  G4VSolid* solidSCFrontClamShell = solidSCFrontClam_0_ebph;
  
  logicScatChamberFrontClamshell = 
    new G4LogicalVolume(solidSCFrontClamShell, GetMaterial("Aluminum"), "SCFrontClamshell_log");
  //new G4LogicalVolume(solidSCFrontClam_0_ebps, GetMaterial("Aluminum"), "SCFrontClamshell_log");
  
  rot_temp = new G4RotationMatrix();
  rot_temp->rotateX(90.0*deg);
  rot_temp->rotateZ(90.0*deg+SCClamAngleApert*0.5-SCWindowAngleOffset);
  
  new G4PVPlacement(rot_temp, G4ThreeVector(0,0,0), logicScatChamberFrontClamshell, 
  		    "SCFrontClamshell", worldlog, false, 0, ChkOverlaps);
  
  // Back Clamshell:
  G4double SCBackClamThick = 0.80*inch;
  G4double SCBackClamHeight = 12.0*inch;
  G4double SCBackClamAngleApert = 145.7*deg;
  G4double SCBackClamAngleOffset = -2.5*deg;
  G4double SCBackClamOuterRadius = SCTankRadius+SCClamThick+SCBackClamThick;
  G4double SCBeamEntranceAngleOffset = SCBackClamAngleOffset-84*deg;
  G4double SCBackViewPipeAngleOffset = SCBackClamAngleOffset-39.9*deg;
  
  G4Tubs* solidSCAddBackClam = 
    new G4Tubs("solidSCAddBackClam", SCTankRadius+SCClamThick-0.5*cm, SCBackClamOuterRadius, 
	       0.5*SCBackClamHeight, 0.0, SCBackClamAngleApert);
  
  rot_temp = new G4RotationMatrix();
  rot_temp->rotateZ(SCBackClamAngleOffset);
  
  G4UnionSolid* solidSCBackClam_0_abc = 
    new G4UnionSolid("solidSCClamshell_0_abc", solidSCClamshell_0, solidSCAddBackClam,
		     rot_temp, G4ThreeVector(0,0,0));
  
  // Entrance beam pipe
  G4Tubs* solidEntranceBeamPipe = 
    new G4Tubs("solidEntranceBeamPipe", 0.0, 2.25*inch, 0.825*inch, 0.0, 360.0*deg);
  
  rot_temp = new G4RotationMatrix();
  rot_temp->rotateX(90.0*deg);
  rot_temp->rotateY(-90.0*deg-SCBeamEntranceAngleOffset);
  
  G4UnionSolid* solidSCBackClam_0_ebp =
    new G4UnionSolid("solidSCClamshell_0_ebp", solidSCBackClam_0_abc, solidEntranceBeamPipe, rot_temp, 
		     G4ThreeVector(SCBackClamOuterRadius*sin(90.0*deg+SCBeamEntranceAngleOffset),
				   SCBackClamOuterRadius*cos(90.0*deg+SCBeamEntranceAngleOffset),
				   0.0)
		     );
  
  G4Tubs* solidEntranceBeamPipeHole = 
    new G4Tubs("solidEntranceBeamPipeHole", 0.0, 1.0*inch, 5.375*inch, 0.0, 360.0*deg);
  
  G4SubtractionSolid* solidSCBackClam_0_ebph =
    new G4SubtractionSolid("solidSCClamshell_0_ebph", solidSCBackClam_0_ebp, 
			   solidEntranceBeamPipeHole, rot_temp, 
			   G4ThreeVector(SCBackClamOuterRadius*
					 sin(90.0*deg+SCBeamEntranceAngleOffset),
					 SCBackClamOuterRadius*
					 cos(90.0*deg+SCBeamEntranceAngleOffset),
					 0.0)
			   );
  
  // Back view pipe
  G4Tubs* solidBackViewPipe = new G4Tubs("solidBackViewPipe", 
					 0.0, 2.12*inch, 1.555*inch, 0.0, 360.0*deg);
  
  rot_temp = new G4RotationMatrix();
  rot_temp->rotateX(90.0*deg);
  rot_temp->rotateY(-90.0*deg-SCBackViewPipeAngleOffset);
  
  G4UnionSolid* solidSCBackClam_0_bvp =
    new G4UnionSolid("solidSCClamshell_0_bvp", solidSCBackClam_0_ebph, 
		     solidBackViewPipe, rot_temp, 
		     G4ThreeVector((SCBackClamOuterRadius+1.501*inch)*sin(90.0*deg+SCBackViewPipeAngleOffset),
				   (SCBackClamOuterRadius+1.501*inch)*cos(90.0*deg+SCBackViewPipeAngleOffset), 
				   0.0)
		     );
  
  G4Tubs* solidBackViewPipeFlange = new G4Tubs("solidBackViewPipeFlange", 
					       0.0, 3.0*inch, 0.5*inch, 0.0, 360.0*deg);
  
  G4UnionSolid* solidSCBackClam_0_bvpf =
    new G4UnionSolid("solidSCClamshell_0_bvpf", solidSCBackClam_0_bvp, 
		     solidBackViewPipeFlange, rot_temp, 
		     G4ThreeVector((SCBackClamOuterRadius+2.556*inch)*sin(90.0*deg+SCBackViewPipeAngleOffset), 
				   (SCBackClamOuterRadius+2.556*inch)*cos(90.0*deg+SCBackViewPipeAngleOffset), 
				   0.0));
  
  G4Tubs* solidBackViewPipeHole = new G4Tubs("solidBackViewPipeHole", 
					     0.0, 2.0*inch, 4.0*inch, 0.0, 360.0*deg);
  
  G4SubtractionSolid* solidSCBackClam_0_bvph =
    new G4SubtractionSolid("solidSCClamshell_0_bvph", solidSCBackClam_0_bvpf, 
			   solidBackViewPipeHole, rot_temp, 
			   G4ThreeVector(SCBackClamOuterRadius*sin(90.0*deg+SCBackViewPipeAngleOffset),
					 SCBackClamOuterRadius*cos(90.0*deg+SCBackViewPipeAngleOffset), 
					 0.0)
			   );
  
  
  // Placing back clamshell
  G4VSolid* solidSCBackClamShell = solidSCBackClam_0_bvph;
    
  logicScatChamberBackClamshell = new G4LogicalVolume(solidSCBackClamShell, 
						      GetMaterial("Aluminum"), 
						      "SCBackClamshell_log");
  
  rot_temp = new G4RotationMatrix();
  rot_temp->rotateX(90.0*deg);
  rot_temp->rotateZ(-90.0*deg+SCClamAngleApert*0.5+SCWindowAngleOffset);
  
  new G4PVPlacement(rot_temp, G4ThreeVector(0,0,0), logicScatChamberBackClamshell, 
  		    "SCBackClamshell", worldlog, false, 0, ChkOverlaps);
  
  // Scattering chamber volume
  //
  G4Tubs* solidScatChamber_0 = new G4Tubs("SC", 0.0, SCRadius, 0.5* SCHeight, 0.0*deg, 360.0*deg);
  
  G4Tubs* solidSCWindowVacuum = 
    new G4Tubs("SCWindowVacuumFront", SCRadius-1.0*cm, SCTankRadius, 0.5*SCWindowHeight, 0.0, SCWindowAngleApert);
  
  rot_temp = new G4RotationMatrix();
  rot_temp->rotateZ(90.0*deg+SCWindowAngleApert*0.5-SCWindowAngleOffset);
  
  G4UnionSolid* solidScatChamber_0_wbv = 
    new G4UnionSolid("solidScatChamber_0_wbv", solidScatChamber_0, solidSCWindowVacuum,
		     rot_temp, G4ThreeVector(0,0,SCOffset));
  
  rot_temp = new G4RotationMatrix();
  rot_temp->rotateZ(-90.0*deg+SCWindowAngleApert*0.5+SCWindowAngleOffset);
  
  G4UnionSolid* solidScatChamber_0_wfv = 
    new G4UnionSolid("solidScatChamber_0_wfv", solidScatChamber_0_wbv, solidSCWindowVacuum,
		     rot_temp, G4ThreeVector(0,0,SCOffset));
  
  rot_temp = new G4RotationMatrix();
  rot_temp->rotateX(90.0*deg);
  G4UnionSolid* solidScatChamber_0_entbp = 
    new G4UnionSolid("solidScatChamber_0_entbp", solidScatChamber_0_wbv, solidEntranceBeamPipeHole, 
		     rot_temp, G4ThreeVector(0, -SCRadius, SCOffset));
  //solidExitBeamPipeHole
  

  G4UnionSolid* solidScatChamber_0_exbp = 
    new G4UnionSolid("solidScatChamber_0_exbp", solidScatChamber_0_entbp, solidExitBeamPipeHole, 
		     rot_temp, G4ThreeVector(0, +SCRadius, SCOffset));
  
  G4VSolid* solidScatChamber = solidScatChamber_0_exbp;

  logicScatChamber = new G4LogicalVolume(solidScatChamber, GetMaterial("Vacuum"), "ScatChamber_log");
  
  new G4PVPlacement(rotSC, *SCPlacement, logicScatChamber, "ScatChamberPhys", worldlog, false, 0, ChkOverlaps);

  fDetCon->InsertTargetVolume( logicScatChamber->GetName() );
  
  rot_temp = new G4RotationMatrix();
  rot_temp->rotateX(90.0*deg);
  
  //Call BuildStandardCryoTarget HERE !
  if(fTargType==G4SBS::kCfoil){
    BuildCfoil(logicScatChamber, rot_temp, G4ThreeVector(0, 0, SCOffset));
  } else if( fTargType == G4SBS::kOptics){
    BuildOpticsTarget(logicScatChamber, rot_temp, G4ThreeVector(0, 0, SCOffset ) );
  } else {
    BuildStandardCryoTarget(logicScatChamber, rot_temp, G4ThreeVector(0, 0, SCOffset));
  }
  
  G4VisAttributes* Invisible  = new G4VisAttributes(G4Colour(0.,0.,0.)); 
  Invisible->SetVisibility(false);
  G4VisAttributes* colourDarkGrey = new G4VisAttributes(G4Colour(0.3,0.3,0.3)); 
  colourDarkGrey->SetForceWireframe(true);
  G4VisAttributes* colourGrey = new G4VisAttributes(G4Colour(0.7,0.7,0.7)); 
  colourGrey->SetForceWireframe(true);
  G4VisAttributes* colourCyan = new G4VisAttributes(G4Colour(0.,1.,1.)); 
  
  logicScatChamberTank->SetVisAttributes(colourDarkGrey);
  logicScatChamberFrontClamshell->SetVisAttributes(colourGrey);
  logicScatChamberLeftSnoutWindow->SetVisAttributes(Invisible);
  logicScatChamberLeftSnoutWindowFrame->SetVisAttributes(colourCyan);
  logicScatChamberRightSnoutWindow->SetVisAttributes(Invisible);
  logicScatChamberRightSnoutWindowFrame->SetVisAttributes(colourCyan);
  logicScatChamberBackClamshell->SetVisAttributes(colourGrey);
  logicScatChamberExitFlangePlate->SetVisAttributes(colourGrey);
  logicScatChamber->SetVisAttributes(Invisible);
  
}

// This function will replaced BuildCryoTarget
// It is identical except it uses the function BuildStandardCryoTarget.
// It is also used for GEp only now, and not for GMn
void G4SBSTargetBuilder::BuildGEpScatCham(G4LogicalVolume *worldlog ){
  // GEP scattering chamber:
  
  // This shall go with Target construction now
  // if( fFlux ){ //Make a sphere to compute particle flux:
  //   G4Sphere *fsph = new G4Sphere( "fsph", 1.5*fTargLen/2.0, 1.5*fTargLen/2.0+cm, 0.0*deg, 360.*deg,
  // 				   0.*deg, 150.*deg );
  //   G4LogicalVolume *fsph_log = new G4LogicalVolume( fsph, GetMaterial("Air"), "fsph_log" );

  //   fsph_log->SetUserLimits(  new G4UserLimits(0.0, 0.0, 0.0, DBL_MAX, DBL_MAX) );
    
  //   new G4PVPlacement( 0, G4ThreeVector(0,0,0), fsph_log, "fsph_phys", worldlog, false, 0 );

  //   G4String FluxSDname = "FLUX";
  //   G4String Fluxcollname = "FLUXHitsCollection";
  //   G4SBSCalSD *FluxSD = NULL;
  //   if( !( FluxSD = (G4SBSCalSD*) fDetCon->fSDman->FindSensitiveDetector(FluxSDname) ) ){
  //     G4cout << "Adding FLUX SD to SDman..." << G4endl;
  //     FluxSD = new G4SBSCalSD( FluxSDname, Fluxcollname );
  //     fDetCon->fSDman->AddNewDetector( FluxSD );
  //     (fDetCon->SDlist).insert( FluxSDname );
  //     fDetCon->SDtype[FluxSDname] = G4SBS::kCAL;

  //     (FluxSD->detmap).depth = 0;
  //   }
  //   fsph_log->SetSensitiveDetector( FluxSD );
  // }
  
  //Start with vacuum snout:
  G4double inch = 2.54*cm;

  //In the following dimensions, "left" and "right" are as viewed from downstream!!!!
  G4double SnoutBeamPlate_Width = 9.425*inch;
  G4double SnoutEarmPlate_Width = 34.364*inch;
  G4double SnoutHarmPlate_Width = 27.831*inch;

  G4double Snout_Height = 37.75*inch;
  G4double Snout_Thick = 1.0*inch;

  //Window dimensions:
  G4double SnoutEarmWindow_Rbend_corners = 5.750*inch;
  G4double SnoutEarmWindow_Width = 2.0*9.625*inch;
  G4double SnoutEarmWindow_Height = 2.0*15.875*inch;
  //G4double SnoutEarmOffset = -0.744*inch;

  G4double SnoutHarmWindow_Rbend_corners = 5.000*inch;
  G4double SnoutHarmWindow_Width = 2.0*8.438*inch;
  G4double SnoutHarmWindow_Height = 2.0*9.562*inch;
  //G4double SnoutRightOffset = -1.4595*inch;

  //Aluminium Dimensions - just need to be bigger than window
  G4double EarmWindowThick = 0.032*inch;
  G4double HarmWindowThick = 0.020*inch;
    
  G4double SnoutEarmWindow_xcenter = SnoutEarmPlate_Width/2.0 - 16.438*inch;
  G4double SnoutHarmWindow_xcenter = -SnoutHarmPlate_Width/2.0 + 15.375*inch;
  G4double SnoutEarmWindowAngle = 27.5*deg;
  G4double SnoutHarmWindowAngle = 22.0*deg;

  G4double SnoutUpstreamHoleDiameter = 4.870*inch; //All the way through:
  G4double SnoutDownstreamHoleDiameter = 5.010*inch; //To a depth of .26 inches from the front:
  G4double SnoutDownstreamHoleDepth = 0.260*inch;

  G4double SnoutBeamPlate_xcenter = SnoutBeamPlate_Width/2.0 - 4.591*inch;

  // x coord. relative to center plate! To get coordinate in hall, we want center hole to be positioned at x = 0:
  G4double SnoutBeamPlate_xcoord = -SnoutBeamPlate_xcenter; // 4.591 - w/2 = -.1215 inch
  G4double SnoutBeamPlate_zcoord = 48.56*inch; //distance to upstream edge of snout beam plate
  //If we keep the target center as the origin of Hall A for detector positioning, then we will have to offset everything else in the
  //target chamber construction
  G4double TargetCenter_zoffset = 6.50*inch; //offset of target center wrt scattering chamber:
  
  //Make Boxes for Snout plates:
  G4Box *SnoutBeamPlate_Box = new G4Box("SnoutBeamPlate_Box", SnoutBeamPlate_Width/2.0, Snout_Height/2.0, Snout_Thick/2.0 );
  G4Box *SnoutEarmPlate_Box = new G4Box("SnoutEarmPlate_Box", SnoutEarmPlate_Width/2.0, Snout_Height/2.0, Snout_Thick/2.0 );
  G4Box *SnoutHarmPlate_Box = new G4Box("SnoutHarmPlate_Box", SnoutHarmPlate_Width/2.0, Snout_Height/2.0, Snout_Thick/2.0 );

  //Make cylinder cutouts for center plate:
  G4Tubs *SnoutBeamPlate_ThroughHole = new G4Tubs( "SnoutBeamPlate_ThroughHole", 0.0, SnoutUpstreamHoleDiameter/2.0, Snout_Thick/2.0 + mm, 0.0, twopi );
  G4Tubs *SnoutBeamPlate_CounterBore = new G4Tubs( "SnoutBeamPlate_CounterBore", 0.0, SnoutDownstreamHoleDiameter/2.0, SnoutDownstreamHoleDepth/2.0 + mm, 0.0, twopi );

  //Make subtraction solid(s) for beam plate:
  G4SubtractionSolid *SnoutBeamPlate_cut1 = new G4SubtractionSolid( "SnoutBeamPlate_cut1", SnoutBeamPlate_Box, SnoutBeamPlate_ThroughHole, 0, G4ThreeVector( SnoutBeamPlate_xcenter, 0, 0 ) );
  // z - (d/2+mm) = t/2 - d --> z = t/2 - d + d/2 + mm = t/2 - d/2 + mm
  G4double zoff_cbore = Snout_Thick/2.0 - SnoutDownstreamHoleDepth/2.0 + mm;
    
  G4SubtractionSolid *SnoutBeamPlate_cut2 = new G4SubtractionSolid( "SnoutBeamPlate_cut2", SnoutBeamPlate_cut1, SnoutBeamPlate_CounterBore, 0, G4ThreeVector( SnoutBeamPlate_xcenter, 0, zoff_cbore ) );

  //Make Window Box cutouts for Earm and Harm plates:
  G4Box *EarmWindowCutout_box = new G4Box("EarmWindowCutout_box", SnoutEarmWindow_Width/2.0, SnoutEarmWindow_Height/2.0, Snout_Thick/2.0+mm );
  G4SubtractionSolid *EarmPlate_cut = new G4SubtractionSolid( "EarmPlate_cut", SnoutEarmPlate_Box, EarmWindowCutout_box, 0, G4ThreeVector( SnoutEarmWindow_xcenter, 0, 0 ) );
							      
  G4Box *HarmWindowCutout_box = new G4Box("HarmWindowCutout_box", SnoutHarmWindow_Width/2.0, SnoutHarmWindow_Height/2.0, Snout_Thick/2.0+mm );
  G4SubtractionSolid *HarmPlate_cut = new G4SubtractionSolid( "HarmPlate_cut", SnoutHarmPlate_Box, HarmWindowCutout_box, 0, G4ThreeVector( SnoutHarmWindow_xcenter, 0, 0 ) );

  //Make rounded corner pieces for later placement (subtraction of box and cylinder):
  G4Box *EarmWindowCorner_box = new G4Box("EarmWindowCorner_box", SnoutEarmWindow_Rbend_corners/2.0, SnoutEarmWindow_Rbend_corners/2.0, Snout_Thick/2.0 );

  G4Tubs *EarmWindowCorner_cyl = new G4Tubs("EarmWindowCorner_cyl", 0.0, SnoutEarmWindow_Rbend_corners, Snout_Thick/2.0 + mm, 0.0, twopi );

  G4SubtractionSolid *EarmWindowCorner = new G4SubtractionSolid( "EarmWindowCorner", EarmWindowCorner_box, EarmWindowCorner_cyl, 0, G4ThreeVector( SnoutEarmWindow_Rbend_corners/2.0, SnoutEarmWindow_Rbend_corners/2.0, 0.0 ) );
  G4LogicalVolume *EarmWindowCorner_log = new G4LogicalVolume( EarmWindowCorner, GetMaterial("Stainless_Steel"), "EarmWindowCorner_log" );
  
  G4Box *HarmWindowCorner_box = new G4Box("HarmWindowCorner_box", SnoutHarmWindow_Rbend_corners/2.0, SnoutHarmWindow_Rbend_corners/2.0, Snout_Thick/2.0 );

  G4Tubs *HarmWindowCorner_cyl = new G4Tubs("HarmWindowCorner_cyl", 0.0, SnoutHarmWindow_Rbend_corners, Snout_Thick/2.0 + mm, 0.0, twopi );

  G4SubtractionSolid *HarmWindowCorner = new G4SubtractionSolid( "HarmWindowCorner", HarmWindowCorner_box, HarmWindowCorner_cyl, 0, G4ThreeVector( SnoutHarmWindow_Rbend_corners/2.0, SnoutHarmWindow_Rbend_corners/2.0, 0.0 ) );
  G4LogicalVolume *HarmWindowCorner_log = new G4LogicalVolume( HarmWindowCorner, GetMaterial("Stainless_Steel"), "HarmWindowCorner_log" );
  
  //Now we want to put together the three plates that make the Snout:
  G4ThreeVector Harm_zaxis( -sin(SnoutHarmWindowAngle), 0, cos(SnoutHarmWindowAngle ) );
  G4ThreeVector Harm_yaxis(0,1,0);
  G4ThreeVector Harm_xaxis( cos(SnoutHarmWindowAngle), 0, sin(SnoutHarmWindowAngle) );

  G4ThreeVector FrontRightCorner_pos_local( -SnoutBeamPlate_Width/2.0, 0, Snout_Thick/2.0 );

  G4ThreeVector HarmPlate_pos_relative = FrontRightCorner_pos_local - Snout_Thick/2.0 * Harm_zaxis - SnoutHarmPlate_Width/2.0 * Harm_xaxis;

  G4ThreeVector Earm_zaxis( sin(SnoutEarmWindowAngle), 0, cos(SnoutEarmWindowAngle) );
  G4ThreeVector Earm_yaxis(0,1,0);
  G4ThreeVector Earm_xaxis( cos(SnoutEarmWindowAngle), 0, -sin(SnoutEarmWindowAngle) );
  
  G4ThreeVector FrontLeftCorner_pos_local( SnoutBeamPlate_Width/2.0, 0, Snout_Thick/2.0 );
  G4ThreeVector EarmPlate_pos_relative = FrontLeftCorner_pos_local - Snout_Thick/2.0 * Earm_zaxis + SnoutEarmPlate_Width/2.0 * Earm_xaxis;

  G4RotationMatrix *rot_harm_window = new G4RotationMatrix;
  rot_harm_window->rotateY( SnoutHarmWindowAngle );

  G4UnionSolid *BeamHarmUnion = new G4UnionSolid( "BeamHarmUnion", SnoutBeamPlate_cut2, HarmPlate_cut, rot_harm_window, HarmPlate_pos_relative );
  G4RotationMatrix *rot_earm_window = new G4RotationMatrix;
  rot_earm_window->rotateY( -SnoutEarmWindowAngle );
  G4UnionSolid *Snout_solid = new G4UnionSolid("Snout_solid", BeamHarmUnion, EarmPlate_cut, rot_earm_window, EarmPlate_pos_relative );
  
  G4LogicalVolume *Snout_log = new G4LogicalVolume( Snout_solid, GetMaterial("Stainless_Steel"), "Snout_log" );

  G4ThreeVector Snout_position_global( SnoutBeamPlate_xcoord, 0, SnoutBeamPlate_zcoord - TargetCenter_zoffset + Snout_Thick/2.0 );

  if( fDetCon->fExpType != G4SBS::kGEPpositron ){ //place snout:
    new G4PVPlacement( 0, Snout_position_global, Snout_log, "Snout_phys", worldlog, false, 0 );
  }
  //Fill the cutouts with vacuum, and then with steel corner pieces:
  G4LogicalVolume *SnoutHarmWindowCutout_log = new G4LogicalVolume( HarmWindowCutout_box, GetMaterial("Vacuum"), "SnoutHarmWindowCutout_log" );
  G4LogicalVolume *SnoutEarmWindowCutout_log = new G4LogicalVolume( EarmWindowCutout_box, GetMaterial("Vacuum"), "SnoutEarmWindowCutout_log" );

  G4RotationMatrix *rot_temp = new G4RotationMatrix;

  G4double xtemp, ytemp;
  xtemp = SnoutHarmWindow_Width/2.0 - SnoutHarmWindow_Rbend_corners/2.0;
  ytemp = SnoutHarmWindow_Height/2.0 - SnoutHarmWindow_Rbend_corners/2.0;

  if( fDetCon->fExpType != G4SBS::kGEPpositron ){
    new G4PVPlacement( 0, G4ThreeVector( -xtemp, -ytemp, 0 ), HarmWindowCorner_log, "HarmWindowCorner_phys_bottom_right", SnoutHarmWindowCutout_log, false, 0 );

    rot_temp->rotateZ( 90.0*deg ); //clockwise as viewed from downstream, ccw as viewed from upstream!
  
    new G4PVPlacement( rot_temp, G4ThreeVector( -xtemp, ytemp, 0 ), HarmWindowCorner_log, "HarmWindowCorner_phys_top_right", SnoutHarmWindowCutout_log, false, 1 );

    rot_temp = new G4RotationMatrix;
    rot_temp->rotateZ( -90.0*deg );

    new G4PVPlacement( rot_temp, G4ThreeVector( xtemp, -ytemp, 0 ), HarmWindowCorner_log, "HarmWindowCorner_phys_bottom_left", SnoutHarmWindowCutout_log, false, 2 );

    rot_temp = new G4RotationMatrix;
    rot_temp->rotateZ( 180.0*deg );

    new G4PVPlacement( rot_temp, G4ThreeVector( xtemp, ytemp, 0 ), HarmWindowCorner_log, "HarmWindowCorner_phys_top_left", SnoutHarmWindowCutout_log, false, 3 );

  
    //Finally, place the window cutout globally:

    G4ThreeVector HarmCutout_pos_global = Snout_position_global + FrontRightCorner_pos_local - Snout_Thick/2.0 * Harm_zaxis - ( SnoutHarmPlate_Width/2.0 - SnoutHarmWindow_xcenter ) * Harm_xaxis;

    new G4PVPlacement( rot_harm_window, HarmCutout_pos_global, SnoutHarmWindowCutout_log, "SnoutHarmWindowCutout_phys", worldlog, false, 0 );

    xtemp = SnoutEarmWindow_Width/2.0 - SnoutEarmWindow_Rbend_corners/2.0;
    ytemp = SnoutEarmWindow_Height/2.0 - SnoutEarmWindow_Rbend_corners/2.0;

    new G4PVPlacement( 0, G4ThreeVector(-xtemp,-ytemp,0), EarmWindowCorner_log, "EarmWindowCorner_phys_bottom_right", SnoutEarmWindowCutout_log, false, 0 );

    rot_temp = new G4RotationMatrix;
    rot_temp->rotateZ(90.0*deg );

    new G4PVPlacement( rot_temp, G4ThreeVector( -xtemp,ytemp,0), EarmWindowCorner_log, "EarmWindowCorner_phys_top_right", SnoutEarmWindowCutout_log, false, 1 );
  
    rot_temp = new G4RotationMatrix;
    rot_temp->rotateZ(-90.0*deg );

    new G4PVPlacement( rot_temp, G4ThreeVector( xtemp,-ytemp,0), EarmWindowCorner_log, "EarmWindowCorner_phys_bottom_left", SnoutEarmWindowCutout_log, false, 2 );

    rot_temp = new G4RotationMatrix;
    rot_temp->rotateZ(180.0*deg );

    new G4PVPlacement( rot_temp, G4ThreeVector( xtemp,ytemp,0), EarmWindowCorner_log, "EarmWindowCorner_phys_top_left", SnoutEarmWindowCutout_log, false, 3 );
  
    G4ThreeVector EarmCutout_pos_global = Snout_position_global + FrontLeftCorner_pos_local - Snout_Thick/2.0 * Earm_zaxis + (SnoutEarmPlate_Width/2.0 + SnoutEarmWindow_xcenter) * Earm_xaxis;

    new G4PVPlacement( rot_earm_window, EarmCutout_pos_global, SnoutEarmWindowCutout_log, "SnoutEarmWindowCutout_phys", worldlog, false, 0 );
  }
  //What's next? Define scattering chamber vacuum volume:
  G4double ScatChamberRadius = 23.80*inch;
  G4double ScatChamberHeight = Snout_Height;

  G4Tubs *ScatChamber_solid = new G4Tubs("ScatChamber_solid", 0, ScatChamberRadius, ScatChamberHeight/2.0, 0.0, twopi );
  G4LogicalVolume *ScatChamber_log = new G4LogicalVolume( ScatChamber_solid, GetMaterial("Vacuum"), "ScatChamber_log" );

  rot_temp = new G4RotationMatrix;
  rot_temp->rotateX( 90.0*deg );

  new G4PVPlacement( rot_temp, G4ThreeVector(0,0,-TargetCenter_zoffset), ScatChamber_log, "ScatChamber_phys", worldlog, false, 0 );

  //Add scattering chamber and all target materials to the list of "TARGET" volumes:
  fDetCon->InsertTargetVolume( ScatChamber_log->GetName() );
  
  rot_temp = new G4RotationMatrix;
  rot_temp->rotateX( -90.0*deg );
  
  //Call BuildStandardCryoTarget HERE !
  BuildStandardCryoTarget(ScatChamber_log, rot_temp, G4ThreeVector(0, -TargetCenter_zoffset, 0));
  
  /*
  //HERE
  //Now let's make a cryotarget:
  G4double Rcell = 4.0*cm;
  G4double uthick = 0.1*mm;
  G4double dthick = 0.15*mm;
  G4double sthick = 0.2*mm;

  G4Tubs *TargetMother_solid = new G4Tubs("TargetMother_solid", 0, Rcell + sthick, (fTargLen+uthick+dthick)/2.0, 0.0, twopi );
  G4LogicalVolume *TargetMother_log = new G4LogicalVolume(TargetMother_solid, GetMaterial("Vacuum"), "TargetMother_log" );
  
  G4Tubs *TargetCell = new G4Tubs("TargetCell", 0, Rcell, fTargLen/2.0, 0, twopi );

  G4LogicalVolume *TargetCell_log;

  if( fTargType == G4SBS::kLH2 ){
  TargetCell_log = new G4LogicalVolume( TargetCell, GetMaterial("LH2"), "TargetCell_log" );
  } else {
  TargetCell_log = new G4LogicalVolume( TargetCell, GetMaterial("LD2"), "TargetCell_log" );
  }
  
  G4Tubs *TargetWall = new G4Tubs("TargetWall", Rcell, Rcell + sthick, fTargLen/2.0, 0, twopi );

  G4LogicalVolume *TargetWall_log = new G4LogicalVolume( TargetWall, GetMaterial("Al"), "TargetWall_log" );

  G4Tubs *UpstreamWindow = new G4Tubs("UpstreamWindow", 0, Rcell + sthick, uthick/2.0, 0, twopi );
  G4Tubs *DownstreamWindow = new G4Tubs("DownstreamWindow", 0, Rcell + sthick, dthick/2.0, 0, twopi );

  G4LogicalVolume *uwindow_log = new G4LogicalVolume( UpstreamWindow, GetMaterial("Al"), "uwindow_log" );
  G4LogicalVolume *dwindow_log = new G4LogicalVolume( DownstreamWindow, GetMaterial("Al"), "dwindow_log" );

  // G4ThreeVector targ_pos(0,0,
  //Now place everything:
  //Need to fix this later: Union solid defining vacuum chamber needs to be defined with the cylinder as the first solid so that we can place the
  //target as a daughter volume at the origin!
  G4double ztemp = -(fTargLen+uthick+dthick)/2.0;
  //place upstream window:
  new G4PVPlacement( 0, G4ThreeVector(0,0,ztemp+uthick/2.0), uwindow_log, "uwindow_phys", TargetMother_log, false, 0 );
  //place target and side walls:
  ztemp += uthick;
  new G4PVPlacement( 0, G4ThreeVector(0,0,ztemp+fTargLen/2.0), TargetCell_log, "TargetCell_phys", TargetMother_log, false, 0 );
  new G4PVPlacement( 0, G4ThreeVector(0,0,ztemp+fTargLen/2.0), TargetWall_log, "TargetWall_phys", TargetMother_log, false, 0 );
  ztemp += fTargLen;
  new G4PVPlacement( 0, G4ThreeVector(0,0,ztemp+dthick/2.0), dwindow_log, "dwindow_phys", TargetMother_log, false, 0 );

  G4double targ_zcenter = (uthick-dthick)/2.0; //position of target center relative to target mother volume
  
  //Compute position of target relative to scattering chamber:
  //The target center should be at
  rot_temp = new G4RotationMatrix;
  rot_temp->rotateX( -90.0*deg );

  //for z of target center to be at zero, 
  
  new G4PVPlacement( rot_temp, G4ThreeVector(0, -(TargetCenter_zoffset-targ_zcenter) ,0), TargetMother_log, "TargetMother_phys", ScatChamber_log, false, 0 );
  */
  
  //Next: Snout vacuum geometry (complicated!)

  //Angles of snout side walls relative to (scattering chamber) origin:
  G4double phisnout_harm = (90.0 - 54.88)*deg; //35.12 degrees
  G4double phisnout_earm = (90.0 - 46.82)*deg; //43.18 degrees

  //The global coordinates of the center of the Earm snout plate:
  G4ThreeVector EarmPlanePos_global = Snout_position_global + EarmPlate_pos_relative - Snout_Thick/2.0 * Earm_zaxis;
  G4ThreeVector HarmPlanePos_global = Snout_position_global + HarmPlate_pos_relative - Snout_Thick/2.0 * Harm_zaxis;

  G4ThreeVector ScatChamberPos_global = G4ThreeVector( 0, 0, -TargetCenter_zoffset );
  
  //The Snout vacuum volume will be the INTERSECTION of a polycone and an extruded solid:
  std::vector<G4TwoVector> snout_polygon;

  xtemp = ScatChamberRadius * sin( phisnout_earm );
  ytemp = ScatChamberRadius * cos( phisnout_earm ) - TargetCenter_zoffset;

  snout_polygon.push_back( G4TwoVector( xtemp, ytemp ) );

  //G4double distance_schbr_origin_to_earm_plane = (EarmPlanePos_global - ScatChamberPos_global).mag(); 

  //Intersection with a line and a plane:
  G4ThreeVector nhat_snout_earm_side( sin( phisnout_earm ), 0, cos( phisnout_earm ) );
  G4ThreeVector nhat_snout_harm_side( -sin( phisnout_harm ), 0, cos( phisnout_harm ) );

  // (r0 + nhat * s - rplane ) dot nplane = 0
  // s = (rplane - r0) dot nplane / (nhat dot nplane )

  G4double s_temp = (EarmPlanePos_global - ScatChamberPos_global).dot( Earm_zaxis )/(nhat_snout_earm_side.dot( Earm_zaxis ) );
  
  G4ThreeVector Snout_earm_side_intercept = ScatChamberPos_global + s_temp * nhat_snout_earm_side;

  xtemp = Snout_earm_side_intercept.x();
  ytemp = Snout_earm_side_intercept.z();

  snout_polygon.push_back( G4TwoVector( xtemp, ytemp ) );

  //inner corner position of snout on earm side:
  G4ThreeVector Snout_earm_corner_pos_rel = Snout_position_global + FrontLeftCorner_pos_local - Snout_Thick*Earm_zaxis + Snout_Thick*tan( SnoutEarmWindowAngle/2.0 ) * Earm_xaxis;
 
  xtemp = Snout_earm_corner_pos_rel.x();
  ytemp = Snout_earm_corner_pos_rel.z();

  snout_polygon.push_back( G4TwoVector( xtemp, ytemp ) );
  
  G4ThreeVector Snout_harm_corner_pos_rel = Snout_position_global + FrontRightCorner_pos_local - Snout_Thick*Harm_zaxis - Snout_Thick*tan( SnoutHarmWindowAngle/2.0 ) * Harm_xaxis;

  xtemp = Snout_harm_corner_pos_rel.x();
  ytemp = Snout_harm_corner_pos_rel.z();

  snout_polygon.push_back( G4TwoVector( xtemp, ytemp ) );
  
  s_temp = (HarmPlanePos_global - ScatChamberPos_global).dot( Harm_zaxis ) / (nhat_snout_harm_side.dot( Harm_zaxis ) );

  G4ThreeVector Snout_harm_side_intercept = ScatChamberPos_global + s_temp * nhat_snout_harm_side;

  xtemp = Snout_harm_side_intercept.x();
  ytemp = Snout_harm_side_intercept.z();

  snout_polygon.push_back( G4TwoVector( xtemp, ytemp ) );

  //last point:
  xtemp = -ScatChamberRadius * sin( phisnout_harm );
  ytemp =  ScatChamberRadius * cos( phisnout_harm ) - TargetCenter_zoffset;

  snout_polygon.push_back( G4TwoVector( xtemp, ytemp ) );

  G4ExtrudedSolid *Snout_pgon = new G4ExtrudedSolid( "Snout_pgon", snout_polygon, Snout_Height/2.0,
						     G4TwoVector(0,0), 1.0,
						     G4TwoVector(0,0), 1.0 );

  //This extruded solid has the x axis to beam left, the yaxis along the beam direction, and the z axis in the -y direction. This means we want to rotate it by 90 degrees ccw about x when looking in the +x direction
  rot_temp = new G4RotationMatrix;
  rot_temp->rotateX( -90.0*deg );

  // G4LogicalVolume *Snout_pgon_log = new G4LogicalVolume( Snout_pgon, GetMaterial("Vacuum"), "Snout_pgon_log" );

  // //Positioning: The origin of this pgon is the same as the origin of the hall, so should be 0,0,0:
  // new G4PVPlacement( rot_temp, G4ThreeVector(0,0,0), Snout_pgon_log, "Snout_pgon_phys", worldlog, false, 0 );

  //Next: make the polycone:
  
  G4double SnoutTaperAngle = 38.81*deg;
  G4double SnoutTaper_zpos = 14.88*inch;

  const G4int nzplanes = 4;
  G4double zplanes[nzplanes] = {-Snout_Height/2.0,
				-(ScatChamberRadius-SnoutTaper_zpos)*tan(SnoutTaperAngle),
				(ScatChamberRadius-SnoutTaper_zpos)*tan(SnoutTaperAngle),
				Snout_Height/2.0 };
  G4double rplanes[nzplanes] = { ScatChamberRadius + (zplanes[1]-zplanes[0])/tan(SnoutTaperAngle),
				 ScatChamberRadius,
				 ScatChamberRadius,
				 ScatChamberRadius + (zplanes[3]-zplanes[2])/tan(SnoutTaperAngle) };

  G4double routplanes[nzplanes] = { ScatChamberRadius + (zplanes[1]-zplanes[0])/tan(SnoutTaperAngle),
				    ScatChamberRadius + (zplanes[1]-zplanes[0])/tan(SnoutTaperAngle),
				    ScatChamberRadius + (zplanes[3]-zplanes[2])/tan(SnoutTaperAngle),
				    ScatChamberRadius + (zplanes[3]-zplanes[2])/tan(SnoutTaperAngle) };

  for( G4int pl=0; pl<nzplanes; ++pl ){
    routplanes[pl] += 40.0*cm;
  }
  
  //We want the z axis of the polycone to coincide with the z axis of the polygon, so to have +x to beam left and +y along beam direction, we need to have +z be vertically down. In this case, the phi start would be
  G4double phistart = 46.82*deg;
  G4double dphi = phisnout_earm + phisnout_harm;

  G4Polycone *SnoutTaper = new G4Polycone( "SnoutTaper", phistart, dphi, nzplanes, zplanes, rplanes, routplanes );
  
  //The polycone has its central axis along the vertical line through the "origin", whereas the polygon has its central axis
  // along the vertical line through the target center;
  //Therefore, we have to offset the position of the polygon in y by +targ z center:
  G4IntersectionSolid *SnoutTaperPgon_intersect = new G4IntersectionSolid( "SnoutTaperPgon_intersect", SnoutTaper, Snout_pgon, 0, G4ThreeVector(0, TargetCenter_zoffset, 0) );

  G4LogicalVolume *SnoutVacuum_log = new G4LogicalVolume( SnoutTaperPgon_intersect, GetMaterial("Vacuum"), "SnoutVacuum_log" );

  new G4PVPlacement( rot_temp, G4ThreeVector(0,0,-TargetCenter_zoffset), SnoutVacuum_log, "SnoutVacuum_phys", worldlog, false, 0 );

  fDetCon->InsertTargetVolume( SnoutVacuum_log->GetName() );
  
  //Iron Tube inside the Snout vacuum volume:
  G4double IronTube_Rmin = 5.0*cm;
  G4double IronTube_Rmax = 7.0*cm;
  G4double IronTube_Thick = 6.5*cm;

  G4Tubs *IronTube = new G4Tubs("IronTube", IronTube_Rmin, IronTube_Rmax, IronTube_Thick/2.0, 0.0, twopi );
  G4LogicalVolume *IronTube_log = new G4LogicalVolume( IronTube, GetMaterial("Iron"), "IronTube_log" );

  IronTube_log->SetVisAttributes( new G4VisAttributes(G4Colour(0.3,0.3,0.3) ) );
  
  //relative to the origin of the snout polycone, the iron tube is at y = snout z - half thick:
  G4ThreeVector IronTube_pos_rel( 0, SnoutBeamPlate_zcoord - IronTube_Thick/2.0, 0 );

  rot_temp = new G4RotationMatrix;
  rot_temp->rotateX( 90.0*deg );

  new G4PVPlacement( rot_temp, IronTube_pos_rel, IronTube_log, "IronTube_phys", SnoutVacuum_log, false, 0 );
  

  //Finally, put windows and bolt plates:

  //if( fDetCon->fExpType != G4SBS::kGEPpositron ){
  
  G4double HarmFlangeWidth = 2.0*11.44*inch;
  G4double HarmFlangeHeight = 2.0*12.56*inch;
  G4double HarmFlangeThick = 0.980*inch;

  G4Box *HarmAlWindow_box = new G4Box("HarmAlWindow_box", HarmFlangeWidth/2.0, HarmFlangeHeight/2.0, HarmWindowThick/2.0 );
  G4LogicalVolume *HarmWindow_log = new G4LogicalVolume( HarmAlWindow_box, GetMaterial("Aluminum"), "HarmWindow_log" );
  //Figure out the placement of the Harm window:
  G4ThreeVector Harm_window_pos = Snout_position_global + FrontRightCorner_pos_local + (-SnoutHarmPlate_Width/2.0 + SnoutHarmWindow_xcenter) * Harm_xaxis + (HarmWindowThick/2.0) * Harm_zaxis;

  if( fDetCon->fExpType != G4SBS::kGEPpositron ) new G4PVPlacement( rot_harm_window, Harm_window_pos, HarmWindow_log, "HarmWindow_phys", worldlog, false, 0 );
  
  G4Box *HarmFlange_box = new G4Box("HarmFlange_box", HarmFlangeWidth/2.0, HarmFlangeHeight/2.0, HarmFlangeThick/2.0 );

  G4SubtractionSolid *HarmFlange_cut = new G4SubtractionSolid( "HarmFlange_cut", HarmFlange_box, HarmWindowCutout_box, 0, G4ThreeVector(0,0,0) );
  G4LogicalVolume *HarmFlange_log = new G4LogicalVolume( HarmFlange_cut, GetMaterial("Aluminum"), "HarmFlange_log" );

  G4ThreeVector HarmFlange_pos = Harm_window_pos + (HarmWindowThick + HarmFlangeThick)/2.0 * Harm_zaxis;

  if( fDetCon->fExpType != G4SBS::kGEPpositron ) new G4PVPlacement( rot_harm_window, HarmFlange_pos, HarmFlange_log, "HarmFlange_phys", worldlog, false, 0 );

  //Create a new logical volume of aluminum instead of steel for the rounded corners of the flange:
  G4LogicalVolume *HarmFlangeCorner_log = new G4LogicalVolume( HarmWindowCorner, GetMaterial("Aluminum"), "HarmFlangeCorner_log" );
  
  G4ThreeVector pos_temp;


  if( fDetCon->fExpType != G4SBS::kGEPpositron ){
    xtemp = SnoutHarmWindow_Width/2.0 - SnoutHarmWindow_Rbend_corners/2.0;
    ytemp = SnoutHarmWindow_Height/2.0 - SnoutHarmWindow_Rbend_corners/2.0;

    rot_temp = new G4RotationMatrix;
    rot_temp->rotateY( SnoutHarmWindowAngle );
  
    pos_temp = HarmFlange_pos - xtemp * Harm_xaxis - ytemp * Harm_yaxis;
  
    new G4PVPlacement( rot_temp, pos_temp, HarmFlangeCorner_log, "HarmFlangeCorner_phys_bottom_right", worldlog, false, 0 );

    rot_temp = new G4RotationMatrix;
    rot_temp->rotateY( SnoutHarmWindowAngle );
    rot_temp->rotateZ( 90.0*deg );
    pos_temp = HarmFlange_pos - xtemp * Harm_xaxis + ytemp * Harm_yaxis;

    new G4PVPlacement( rot_temp, pos_temp, HarmFlangeCorner_log, "HarmFlangeCorner_phys_top_right", worldlog, false, 1 );

    rot_temp = new G4RotationMatrix;
    rot_temp->rotateY( SnoutHarmWindowAngle );
    rot_temp->rotateZ( -90.0*deg );
    pos_temp = HarmFlange_pos + xtemp * Harm_xaxis - ytemp * Harm_yaxis;

    new G4PVPlacement( rot_temp, pos_temp, HarmFlangeCorner_log, "HarmFlangeCorner_phys_bottom_left", worldlog, false, 2 );

    rot_temp = new G4RotationMatrix;
    rot_temp->rotateY( SnoutHarmWindowAngle );
    rot_temp->rotateZ( 180.0*deg );
    pos_temp = HarmFlange_pos + xtemp * Harm_xaxis + ytemp * Harm_yaxis;

    new G4PVPlacement( rot_temp, pos_temp, HarmFlangeCorner_log, "HarmFlangeCorner_phys_top_left", worldlog, false, 3 );
  }
    
  G4double EarmFlangeWidth = 2.0*12.62*inch;
  G4double EarmFlangeHeight = Snout_Height;
  G4double EarmFlangeThick = 0.980*inch;

  G4Box *EarmAlWindow_box = new G4Box( "EarmAlWindow_box", EarmFlangeWidth/2.0, EarmFlangeHeight/2.0, EarmWindowThick/2.0 );

  G4LogicalVolume *EarmWindow_log = new G4LogicalVolume( EarmAlWindow_box, GetMaterial("Aluminum"), "EarmWindow_log" );
  G4ThreeVector Earm_window_pos = Snout_position_global + FrontLeftCorner_pos_local + (SnoutEarmPlate_Width/2.0 + SnoutEarmWindow_xcenter) * Earm_xaxis + EarmWindowThick/2.0 * Earm_zaxis;

  
  if( fDetCon->fExpType != G4SBS::kGEPpositron ) new G4PVPlacement( rot_earm_window, Earm_window_pos, EarmWindow_log, "EarmWindow_phys", worldlog, false, 0 );
  
  //Next: make flange:

  G4Box *EarmFlangeBox = new G4Box( "EarmFlangeBox", EarmFlangeWidth/2.0, EarmFlangeHeight/2.0, EarmFlangeThick/2.0 );
  G4SubtractionSolid *EarmFlange_cut = new G4SubtractionSolid( "EarmFlange_cut", EarmFlangeBox, EarmWindowCutout_box, 0, G4ThreeVector(0,0,0));

  G4LogicalVolume *EarmFlange_log = new G4LogicalVolume( EarmFlange_cut, GetMaterial("Aluminum"), "EarmFlange_log" );

  G4ThreeVector EarmFlange_pos = Earm_window_pos + (EarmWindowThick + EarmFlangeThick)/2.0 * Earm_zaxis;

  if( fDetCon->fExpType != G4SBS::kGEPpositron ) new G4PVPlacement( rot_earm_window, EarmFlange_pos, EarmFlange_log, "EarmFlange_phys", worldlog, false, 0 );

  G4LogicalVolume *EarmFlangeCorner_log = new G4LogicalVolume( EarmWindowCorner, GetMaterial("Aluminum"), "EarmFlangeCorner_log" );


  if( fDetCon->fExpType != G4SBS::kGEPpositron ){
    xtemp = SnoutEarmWindow_Width/2.0 - SnoutEarmWindow_Rbend_corners/2.0;
    ytemp = SnoutEarmWindow_Height/2.0 - SnoutEarmWindow_Rbend_corners/2.0;

    rot_temp = new G4RotationMatrix;
    rot_temp->rotateY( -SnoutEarmWindowAngle );
  
    pos_temp = EarmFlange_pos - xtemp * Earm_xaxis - ytemp * Earm_yaxis;
  
    new G4PVPlacement( rot_temp, pos_temp, EarmFlangeCorner_log, "EarmFlangeCorner_phys_bottom_right", worldlog, false, 0 );

    rot_temp = new G4RotationMatrix;
    rot_temp->rotateY( -SnoutEarmWindowAngle );
    rot_temp->rotateZ( 90.0*deg );
    pos_temp = EarmFlange_pos - xtemp * Earm_xaxis + ytemp * Earm_yaxis;
  
    new G4PVPlacement( rot_temp, pos_temp, EarmFlangeCorner_log, "EarmFlangeCorner_phys_top_right", worldlog, false, 1 );

    rot_temp = new G4RotationMatrix;
    rot_temp->rotateY( -SnoutEarmWindowAngle );
    rot_temp->rotateZ( -90.0*deg );
    pos_temp = EarmFlange_pos + xtemp * Earm_xaxis - ytemp * Earm_yaxis;
  
    new G4PVPlacement( rot_temp, pos_temp, EarmFlangeCorner_log, "EarmFlangeCorner_phys_bottom_left", worldlog, false, 2 );

    rot_temp = new G4RotationMatrix;
    rot_temp->rotateY( -SnoutEarmWindowAngle );
    rot_temp->rotateZ( 180.0*deg );
    pos_temp = EarmFlange_pos + xtemp * Earm_xaxis + ytemp * Earm_yaxis;
  
    new G4PVPlacement( rot_temp, pos_temp, EarmFlangeCorner_log, "EarmFlangeCorner_phys_top_left", worldlog, false, 3 );
  }
  //}
  // VISUALS
  // Mother Volumes

  // Vacuum

  G4VisAttributes *Snout_VisAtt = new G4VisAttributes( G4Colour( 0.6, 0.55, 0.65 ) );
  Snout_log->SetVisAttributes( Snout_VisAtt );
  HarmWindowCorner_log->SetVisAttributes( Snout_VisAtt );
  EarmWindowCorner_log->SetVisAttributes( Snout_VisAtt );

  G4VisAttributes *Flange_VisAtt = new G4VisAttributes( G4Colour( 0.15, 0.8, 0.9 ) );
  HarmFlange_log->SetVisAttributes( Flange_VisAtt );
  HarmFlangeCorner_log->SetVisAttributes( Flange_VisAtt );

  EarmFlange_log->SetVisAttributes( Flange_VisAtt );
  EarmFlangeCorner_log->SetVisAttributes( Flange_VisAtt );
  // PlateUnionLog->SetVisAttributes( Snout_VisAtt );
  // LeftCornerLog->SetVisAttributes( Snout_VisAtt );
  // RightCornerLog->SetVisAttributes( Snout_VisAtt );
  // RightWindowCutoutVacuum_log->SetVisAttributes( G4VisAttributes::Invisible );
  // LeftWindowCutoutVacuum_log->SetVisAttributes( G4VisAttributes::Invisible );

  SnoutHarmWindowCutout_log->SetVisAttributes( G4VisAttributes::Invisible );
  SnoutEarmWindowCutout_log->SetVisAttributes( G4VisAttributes::Invisible );
  
  G4VisAttributes *Snout_VisAttWire = new G4VisAttributes( G4Colour( 0.6, 0.55, 0.65 ) );
  Snout_VisAttWire->SetForceWireframe(true);

  G4VisAttributes *Window_visatt = new G4VisAttributes( G4Colour( 0.9, 0.8, 0.15 ) );
  Window_visatt->SetForceWireframe(true);
  HarmWindow_log->SetVisAttributes( Window_visatt );
  EarmWindow_log->SetVisAttributes( Window_visatt );
  //Snout_ScChamb->SetVisAttributes( Snout_VisAttWire );

  ScatChamber_log->SetVisAttributes( Snout_VisAttWire );
  SnoutVacuum_log->SetVisAttributes( Snout_VisAttWire );
  
  // G4VisAttributes *Targ_visatt = new G4VisAttributes( G4Colour( 0.1, 0.05, 0.9 ) );
  // TargetCell_log->SetVisAttributes( Targ_visatt );
  // G4VisAttributes *TargWall_visatt = new G4VisAttributes( G4Colour( 0.9, .05, 0.1 ) );
  // TargWall_visatt->SetForceWireframe( true );
  // TargetWall_log->SetVisAttributes( TargWall_visatt );
  // uwindow_log->SetVisAttributes( TargWall_visatt );
  // dwindow_log->SetVisAttributes( TargWall_visatt );
  // TargetMother_log->SetVisAttributes( G4VisAttributes::Invisible );
  
  G4VisAttributes *AlColor= new G4VisAttributes(G4Colour(0.4,0.4,0.4));
  //LeftAl_Log->SetVisAttributes( AlColor );
  //RightAl_Log->SetVisAttributes( AlColor );

  G4VisAttributes *ironColor= new G4VisAttributes(G4Colour(0.52,0.47,0.47));
  // TUB1_log->SetVisAttributes( ironColor );
  // TUB2_log->SetVisAttributes( ironColor );
  // TUB3_log->SetVisAttributes( ironColor );
}

// This function has replaced BuildC16CryoTarget
// It is identical except it uses the function BuildStandardCryoTarget.
void G4SBSTargetBuilder::BuildC16ScatCham(G4LogicalVolume *worldlog ){
  G4RotationMatrix *rot_temp;
  
  G4double inch = 2.54*cm;
  
  G4double entpipe_rin = 31.75*mm;
  G4double entpipe_rout = entpipe_rin + 0.12*mm;

  double sheight = 1.2*m;
  double swinthick    = 0.38*mm;
  double swallrad     = 1.143*m/2.0;
  double swallrad_in  = 1.041*m/2.0;
  
  double hcal_ang_min = -55.0*deg;
  double hcal_ang_max =  45.0*deg;
  double hcal_win_h = 15.2*cm;

  double bb_ang_min = 18.0*deg;
  double bb_ang_max = 80.0*deg;
  double bb_win_h = 0.5*m;

  //G4double extpipe_rin = 48.00*mm;
  G4double extpipe_rin = 6.0*inch/2.0;
  G4double extpipestart = 1.622*m;
  
  //  if( bb_ang_min < hcal_ang_max ) bb_ang_min = hcal_ang_max + (swallrad-swallrad_in)/swallrad/4;
  G4double dvcs_ang_min = 8.0*deg;
  G4double dvcs_ang_max = (90.0+52.0)*deg; //142 deg

  G4double angthick_dvcs_snout = 2.5*deg; 
  G4double Rin_dvcs_snout = 29.315*inch;
  G4double Rout_dvcs_snout = 30.315*inch;

  G4double extpipe_len = extpipestart - Rin_dvcs_snout;
  
  G4double height_dvcs_snout = 12.5*inch;
  G4double Rin_dvcs_beampipe = 6.065*inch/2.0;
  G4double Rout_dvcs_beampipe = Rin_dvcs_beampipe + 0.28*inch;
  
  // Make the chamber
  G4Tubs *swall = new G4Tubs("scham_wall", swallrad_in, swallrad, sheight/2.0, 0.0*deg, 360.0*deg );
  G4Tubs *dvcs_snout_cut = new G4Tubs("dvcs_snout_cut", swallrad_in - 1.0*cm, swallrad + 1.0*cm, height_dvcs_snout/2.0, dvcs_ang_min, dvcs_ang_max-dvcs_ang_min );
  
  // Cut out for windows
  // G4Tubs *swall_hcalcut = new G4Tubs("scham_wall_hcalcut", swallrad_in-2*cm, swallrad+2*cm, hcal_win_h, hcal_ang_min, hcal_ang_max-hcal_ang_min);
  // G4Tubs *swall_bbcut = new G4Tubs("scham_wall_bbcut", swallrad_in-2*cm, swallrad+2*cm, bb_win_h, bb_ang_min, bb_ang_max-bb_ang_min);

  //  G4Tubs *bigdvcscut = new G4Tubs("bigdvcscut", 

  rot_temp = new G4RotationMatrix;
  rot_temp->rotateX(-90.0*deg);
  rot_temp->rotateY(180.0*deg);
  
  G4SubtractionSolid *swallcut1 = new G4SubtractionSolid( "swallcut1", swall, dvcs_snout_cut );

  G4Tubs *upstream_beam_hole = new G4Tubs("upstream_beam_hole", 0.0, entpipe_rin, 5.0*inch, 0, 360.*deg);
  G4SubtractionSolid *swallcut2 = new G4SubtractionSolid( "swallcut2", swallcut1, upstream_beam_hole, rot_temp, G4ThreeVector(0,-0.5*(swallrad+swallrad_in),0) );
  
  G4Tubs *dvcs_snout = new G4Tubs("dvcs_snout", Rin_dvcs_snout, Rout_dvcs_snout, height_dvcs_snout/2.0, dvcs_ang_min - angthick_dvcs_snout, dvcs_ang_max - dvcs_ang_min + 2.*angthick_dvcs_snout );

  G4Tubs *dvcs_beamleft_cut = new G4Tubs("dvcs_beamleft_cut", 0.0, Rout_dvcs_snout + cm, 5.0*inch/2.0, 100.6*deg, 38.8*deg );

  G4Tubs *dvcs_beamright_cut = new G4Tubs("dvcs_beamright_cut", 0.0, Rout_dvcs_snout + cm, 5.0*inch/2.0, 10.6*deg, 47.6*deg );

  G4SubtractionSolid *dvcs_snout_cut1 = new G4SubtractionSolid( "dvcs_snout_cut1", dvcs_snout, dvcs_beamleft_cut );
  G4SubtractionSolid *dvcs_snout_cut2 = new G4SubtractionSolid( "dvcs_snout_cut2", dvcs_snout_cut1, dvcs_beamright_cut );

  G4Tubs *dvcs_snout_vacuum = new G4Tubs("dvcs_snout_vacuum", swallrad, Rout_dvcs_snout, height_dvcs_snout/2.0, dvcs_ang_min - angthick_dvcs_snout, dvcs_ang_max - dvcs_ang_min + 2.*angthick_dvcs_snout );

  G4LogicalVolume *dvcs_snout_vacuum_log = new G4LogicalVolume(dvcs_snout_vacuum, GetMaterial("Vacuum"), "dvcs_snout_vacuum_log" );

  fDetCon->InsertTargetVolume( dvcs_snout_vacuum_log->GetName() );
  
  G4Tubs *dvcs_snout_beamhole = new G4Tubs("dvcs_snout_beamhole", 0.0, Rin_dvcs_beampipe, 5.0*inch, 0.0, 360.0*deg );

  
  
  G4SubtractionSolid *dvcs_snout_cut3 = new G4SubtractionSolid( "dvcs_snout_cut3", dvcs_snout_cut2, dvcs_snout_beamhole, rot_temp, G4ThreeVector( 0.0, 0.5*(Rin_dvcs_snout+Rout_dvcs_snout), 0.0) );

  G4LogicalVolume *dvcs_snout_log = new G4LogicalVolume(dvcs_snout_cut3, GetMaterial("Aluminum"), "dvcs_snout_log");

  new G4PVPlacement( 0, G4ThreeVector(0,0,0), dvcs_snout_log, "dvcs_snout_phys", dvcs_snout_vacuum_log, false, 0 );

  new G4PVPlacement( rot_temp, G4ThreeVector(0,0,0), dvcs_snout_vacuum_log, "dvcs_snout_vacuum_phys", worldlog, false, 0);

  G4double dvcs_win_thick = (30.331-30.315)*inch;

  G4Tubs *dvcs_beamleft_window = new G4Tubs("dvcs_beamleft_window", Rout_dvcs_snout, Rout_dvcs_snout + dvcs_win_thick, 7.328*inch/2.0, (100.6-2.23)*deg, (38.8+4.46)*deg );
  G4LogicalVolume *dvcs_beamleft_window_log = new G4LogicalVolume( dvcs_beamleft_window, GetMaterial("Aluminum"), "dvcs_beamleft_window_log" );

  new G4PVPlacement( rot_temp, G4ThreeVector(0,0,0), dvcs_beamleft_window_log, "dvcs_beamleft_window_phys", worldlog, false, 0 );

  G4Tubs *dvcs_beamright_window = new G4Tubs("dvcs_beamright_window", Rout_dvcs_snout, Rout_dvcs_snout + dvcs_win_thick, 7.328*inch/2.0, 8.0*deg, (47.6+4.4)*deg );
  G4LogicalVolume *dvcs_beamright_window_log = new G4LogicalVolume( dvcs_beamright_window, GetMaterial("Aluminum"), "dvcs_beamright_window_log" );

  new G4PVPlacement( rot_temp, G4ThreeVector(0,0,0), dvcs_beamright_window_log, "dvcs_beamright_window_phys", worldlog, false, 0 );
  
  G4VisAttributes *window_visatt = new G4VisAttributes( G4Colour( 0.8, 0.8, 0.0 ) );
  window_visatt->SetForceWireframe(true);
  dvcs_beamleft_window_log->SetVisAttributes( window_visatt );
  dvcs_beamright_window_log->SetVisAttributes( window_visatt );
  
  G4Tubs *scham_vacuum = new G4Tubs("scham_vacuum", 0.0, swallrad, sheight/2.0, 0.0, 360.0*deg );
  G4LogicalVolume *scham_vacuum_log = new G4LogicalVolume( scham_vacuum, GetMaterial("Vacuum"), "scham_vacuum_log" );
  G4LogicalVolume *scham_wall_log = new G4LogicalVolume( swallcut2, GetMaterial("Aluminum"), "scham_wall_log" );

  new G4PVPlacement( 0, G4ThreeVector(0,0,0), scham_wall_log, "scham_wall_phys", scham_vacuum_log, false, 0 );
  new G4PVPlacement( rot_temp, G4ThreeVector(0,0,0), scham_vacuum_log, "scham_vacuum_phys", worldlog, false, 0 );

  fDetCon->InsertTargetVolume( scham_vacuum_log->GetName() );
  
  //swallcut = new G4SubtractionSolid("swallcut2", swallcut, swall_bbcut);

  // G4Tubs *swall_hcalwin = new G4Tubs("scham_wall_hcalwin", swallrad_in, swallrad_in+swinthick, hcal_win_h, hcal_ang_min, hcal_ang_max-hcal_ang_min);
  // G4Tubs *swall_bbwin = new G4Tubs("scham_wall_bbwin", swallrad_in, swallrad_in+swinthick, bb_win_h, bb_ang_min, bb_ang_max-bb_ang_min)
  // Exit pipe, prepping to bore out holes in scattering chamber
  
  // G4Tubs *exttube = new G4Tubs("exitpipetube", extpipe_rin, extpipe_rin+0.28*inch, extpipe_len/2, 0.*deg, 360.*deg );
  // G4Tubs *extvactube = new G4Tubs("exitpipetube_vac", 0.0, extpipe_rin, extpipe_len, 0.*deg, 360.*deg );

  // G4LogicalVolume *extpipe_log = new G4LogicalVolume(exttube, GetMaterial("Aluminum"),"extpipe_log");
  // G4LogicalVolume *extvac_log = new G4LogicalVolume(extvactube, GetMaterial("Vacuum"),"extvac_log");

  // double tempZ = 5.0*cm + 1*cm;
  // G4Tubs *swall_enthole = new G4Tubs("scham_wall_enthole", 0.0, entpipe_rout, tempZ, 0.0*deg, 360.0*deg );
  // G4Tubs *swall_exthole = new G4Tubs("scham_wall_exthole", 0.0, extpipe_rin, tempZ, 0.*deg, 360.*deg );

  // G4RotationMatrix *chamholerot = new G4RotationMatrix;
  // chamholerot->rotateY(90.0*deg);

  //  Cut holes in the scattering chamber and HCal aluminum window 
  // G4SubtractionSolid* swall_holes = new G4SubtractionSolid("swall_enthole", swallcut, swall_enthole, chamholerot, G4ThreeVector(-(swallrad+swallrad_in)/2.0, 0.0, 0.0) );
  // swall_holes = new G4SubtractionSolid("swall_holes", swall_holes, swall_exthole, chamholerot, G4ThreeVector((swallrad+swallrad_in)/2, 0.0, 0.0) );

  // G4SubtractionSolid *swall_hcalwin_cut = new G4SubtractionSolid("swall_hcalwin_cut", swall_hcalwin, swall_exthole, chamholerot, G4ThreeVector((swallrad+swallrad_in-5*cm)/2.0,0,0) );

  // // Fill the Entry / Exit holes with vacuum, and place them 
  // tempZ = 5.1*cm;
  // swall_enthole = new G4Tubs("scham_wall_enthole", 0.0, entpipe_rout, tempZ/2.0, 0.0*deg, 360.0*deg );
  // G4LogicalVolume *sc_entry_hole_vacuum_log = new G4LogicalVolume( swall_enthole, GetMaterial("Vacuum"), "sc_entry_hole_vacuum_log");
  // new G4PVPlacement(0, G4ThreeVector(0.0, 0.0, -(swallrad+swallrad_in)/2.0), sc_entry_hole_vacuum_log, "sc_entry_hole_vacuum_phys",
  // 		    worldlog, false, 0 );

  // swall_exthole = new G4Tubs("scham_wall_exthole", 0.0, extpipe_rin, tempZ/2.0, 0.0*deg, 360.0*deg );
  // G4LogicalVolume *sc_exit_hole_vacuum_log = new G4LogicalVolume( swall_exthole, GetMaterial("Vacuum"), "sc_exit_hole_vacuum_log" );
  // new G4PVPlacement(0, G4ThreeVector(0.0, 0.0, (swallrad+swallrad_in)/2.0), sc_exit_hole_vacuum_log, "sc_exit_hole_vacuum_phys",
  // 		    worldlog, false, 0 );

  // // Turn all subtraction solids into Logical Volumes of the appropriate material.
  // G4LogicalVolume *swall_log = new G4LogicalVolume(swall_holes, GetMaterial("Aluminum"),"scham_wall_log");
  // G4LogicalVolume *sc_hcalwin_log = new G4LogicalVolume(swall_hcalwin_cut, GetMaterial("Aluminum"),"sc_hcalwin_log");
  // G4LogicalVolume *sc_bbwin_log = new G4LogicalVolume(swall_bbwin, GetMaterial("Aluminum"),"sc_bbwin_log");

  // G4RotationMatrix *schamrot = new G4RotationMatrix;
  // schamrot->rotateX(-90.0*deg);
  // schamrot->rotateZ(-90.0*deg);

  // // Fill Scattering Chamber with Vacuum
  // G4Tubs *chamber_inner = new G4Tubs("chamber_inner", 0.0, swallrad_in,  sheight/2, 0.0*deg, 360.0*deg );
  // G4LogicalVolume* chamber_inner_log = new G4LogicalVolume(chamber_inner, GetMaterial("Vacuum"), "cham_inner_log");

  // Top and bottom
  G4Tubs *sc_topbottom = new G4Tubs("scham_topbottom", 0.0, swallrad, (swallrad-swallrad_in)/2.0, 0.0*deg, 360.0*deg );
  G4LogicalVolume* sc_topbottom_log = new G4LogicalVolume(sc_topbottom, GetMaterial("Aluminum"), "scham_topbottom_log");

  //////////////////////////////////////////////////////////

  // // Scattering chamber
  // new G4PVPlacement(schamrot, G4ThreeVector(0.0, 0.0, 0.0), swall_log,
  // 		    "scham_wall_phys", worldlog, false, 0);

  // new G4PVPlacement(schamrot, G4ThreeVector(0.0, 0.0, 0.0), chamber_inner_log,
  // 		    "chamber_inner_phys", worldlog, false, 0);

  // new G4PVPlacement(schamrot, G4ThreeVector(0.0, 0.0, 0.0), sc_hcalwin_log,
  // 		    "sc_hcalwin_phys", worldlog, false, 0);

  new G4PVPlacement(rot_temp, G4ThreeVector(0.0, sheight/2.0 + (swallrad-swallrad_in)/2, 0.0), sc_topbottom_log,
  		    "scham_top_phys", worldlog, false, 0);

  new G4PVPlacement(rot_temp, G4ThreeVector(0.0, -sheight/2.0 - (swallrad-swallrad_in)/2, 0.0), sc_topbottom_log,
  		    "scham_bot_phys", worldlog, false, 0);

  // new G4PVPlacement(0, G4ThreeVector(0.0, 0.0, extpipestart-extpipe_len/2), extpipe_log, "extpipe_phys", worldlog, false, 0);
  // new G4PVPlacement(0, G4ThreeVector(0.0, 0.0, extpipestart-extpipe_len/2), extvac_log, "extvacpipe_phys", worldlog, false, 0);

  //Make exit beam pipe and vacuum:
  G4Tubs *exit_pipe = new G4Tubs( "exit_pipe", Rin_dvcs_beampipe, Rout_dvcs_beampipe, extpipe_len/2.0, 0.0, 360.*deg );
  G4Tubs *exit_vacuum = new G4Tubs( "exit_vacuum", 0.0, Rin_dvcs_beampipe, extpipe_len/2.0, 0.0, 360.*deg );

  //Cut the exit pipe and exit vacuum using dvcs vacuum snout:
  G4double z0_exitpipe = 0.5*(extpipestart + Rin_dvcs_snout);

  G4RotationMatrix rinv = rot_temp->inverse();
  
  G4SubtractionSolid *exit_pipe_cut = new G4SubtractionSolid( "exit_pipe_cut", exit_pipe, dvcs_snout_vacuum, &rinv, G4ThreeVector(0,0,-z0_exitpipe) );

  G4SubtractionSolid *exit_vacuum_cut = new G4SubtractionSolid( "exit_vacuum_cut", exit_vacuum, dvcs_snout_vacuum, &rinv, G4ThreeVector(0,0,-z0_exitpipe ) );

  G4LogicalVolume *exit_pipe_log = new G4LogicalVolume( exit_pipe_cut, GetMaterial("Aluminum"), "exit_pipe_log" );
  G4LogicalVolume *exit_vacuum_log = new G4LogicalVolume( exit_vacuum_cut, GetMaterial("Vacuum"), "exit_vacuum_log" );
  new G4PVPlacement( 0, G4ThreeVector(0,0,z0_exitpipe ), exit_pipe_log, "exit_pipe_phys", worldlog, false, 0 );
  new G4PVPlacement( 0, G4ThreeVector(0,0,z0_exitpipe ), exit_vacuum_log, "exit_vacuum_phys", worldlog, false, 0 );
  
  rot_temp = new G4RotationMatrix();
  rot_temp->rotateX(+90.0*deg);
  
  //Call BuildStandardCryoTarget HERE !
  if(fTargType==G4SBS::kCfoil){
    BuildCfoil(scham_vacuum_log, rot_temp, G4ThreeVector(0, +0.025*mm, 0));
  }else{
    BuildStandardCryoTarget(scham_vacuum_log, rot_temp, G4ThreeVector(0, +0.025*mm, 0));
  }
  /*
  // Now let's make a cryotarget:
  G4double Rcell = 4.0*cm;
  G4double uthick = 0.1*mm;
  G4double dthick = 0.15*mm;
  G4double sthick = 0.2*mm;

  G4Tubs *TargetMother_solid = new G4Tubs( "TargetMother_solid", 0, Rcell + sthick, (fTargLen+uthick+dthick)/2.0, 0.0, twopi );
  G4LogicalVolume *TargetMother_log = new G4LogicalVolume( TargetMother_solid, GetMaterial("Vacuum"), "TargetMother_log" );
  
  G4Tubs *TargetCell = new G4Tubs( "TargetCell", 0, Rcell, fTargLen/2.0, 0, twopi );

  G4LogicalVolume *TargetCell_log;

  if( fTargType == G4SBS::kLH2 ){
  TargetCell_log = new G4LogicalVolume( TargetCell, GetMaterial("LH2"), "TargetCell_log" );
  } else {
  TargetCell_log = new G4LogicalVolume( TargetCell, GetMaterial("LD2"), "TargetCell_log" );
  }

  G4Tubs *TargetWall = new G4Tubs("TargetWall", Rcell, Rcell + sthick, fTargLen/2.0, 0, twopi );

  G4LogicalVolume *TargetWall_log = new G4LogicalVolume( TargetWall, GetMaterial("Al"), "TargetWall_log" );

  G4Tubs *UpstreamWindow = new G4Tubs("UpstreamWindow", 0, Rcell + sthick, uthick/2.0, 0, twopi );
  G4Tubs *DownstreamWindow = new G4Tubs("DownstreamWindow", 0, Rcell + sthick, dthick/2.0, 0, twopi );

  G4LogicalVolume *uwindow_log = new G4LogicalVolume( UpstreamWindow, GetMaterial("Al"), "uwindow_log" );
  G4LogicalVolume *dwindow_log = new G4LogicalVolume( DownstreamWindow, GetMaterial("Al"), "dwindow_log" );

  // Now place everything:

  G4double ztemp = -(fTargLen+uthick+dthick)/2.0;
  // Place upstream window:
  new G4PVPlacement( 0, G4ThreeVector(0,0,ztemp+uthick/2.0), uwindow_log, "uwindow_phys", TargetMother_log, false, 0 );
  // Place target and side walls:
  ztemp += uthick;
  new G4PVPlacement( 0, G4ThreeVector(0,0,ztemp+fTargLen/2.0), TargetCell_log, "TargetCell_phys", TargetMother_log, false, 0 );
  new G4PVPlacement( 0, G4ThreeVector(0,0,ztemp+fTargLen/2.0), TargetWall_log, "TargetWall_phys", TargetMother_log, false, 0 );
  ztemp += fTargLen;
  new G4PVPlacement( 0, G4ThreeVector(0,0,ztemp+dthick/2.0), dwindow_log, "dwindow_phys", TargetMother_log, false, 0 );
  
  // Place target at origin of the Scattering Chamber 
  G4RotationMatrix *targrot = new G4RotationMatrix;
  targrot->rotateX(90.0*deg);
  new G4PVPlacement( targrot, G4ThreeVector(0.0, 0.0,0.0), TargetMother_log, "TargetMother_phys", scham_vacuum_log, false, 0 );

  if( fFlux ){ //Make a sphere to compute particle flux:
  G4Sphere *fsph = new G4Sphere( "fsph", 1.5*fTargLen/2.0, 1.5*fTargLen/2.0+cm, 0.0*deg, 360.*deg,
  0.*deg, 150.*deg );
  G4LogicalVolume *fsph_log = new G4LogicalVolume( fsph, GetMaterial("Air"), "fsph_log" );
  new G4PVPlacement( targrot, G4ThreeVector(0,0,0), fsph_log, "fsph_phys", scham_vacuum_log, false, 0 );
    
  G4String FluxSDname = "FLUX";
  G4String Fluxcollname = "FLUXHitsCollection";
  G4SBSCalSD *FluxSD = NULL;
  if( !( FluxSD = (G4SBSCalSD*) fDetCon->fSDman->FindSensitiveDetector(FluxSDname) ) ){
  G4cout << "Adding FLUX SD to SDman..." << G4endl;
  FluxSD = new G4SBSCalSD( FluxSDname, Fluxcollname );
  fDetCon->fSDman->AddNewDetector( FluxSD );
  (fDetCon->SDlist).insert( FluxSDname );
  fDetCon->SDtype[FluxSDname] = G4SBS::kCAL;
    
  (FluxSD->detmap).depth = 0;
  }
  fsph_log->SetSensitiveDetector( FluxSD );
  fsph_log->SetUserLimits(  new G4UserLimits(0.0, 0.0, 0.0, DBL_MAX, DBL_MAX) );
  }
  */
  
  //  VISUALS
  G4VisAttributes * schamVisAtt = new G4VisAttributes(G4Colour(0.7,0.7,1.0));
  schamVisAtt->SetForceWireframe(true);
  scham_wall_log->SetVisAttributes(schamVisAtt);
  scham_vacuum_log->SetVisAttributes(G4VisAttributes::Invisible);
  sc_topbottom_log->SetVisAttributes(schamVisAtt);
  dvcs_snout_vacuum_log->SetVisAttributes( schamVisAtt );
  // chamber_inner_log->SetVisAttributes(G4VisAttributes::Invisible);
  // sc_entry_hole_vacuum_log->SetVisAttributes( G4VisAttributes::Invisible );
  // sc_exit_hole_vacuum_log->SetVisAttributes( G4VisAttributes::Invisible );

  // G4VisAttributes *pipeVisAtt= new G4VisAttributes(G4Colour(0.6,0.6,0.6));
  // extpipe_log->SetVisAttributes(pipeVisAtt);
  // extvac_log->SetVisAttributes( G4VisAttributes::Invisible );

  // G4VisAttributes *winVisAtt = new G4VisAttributes(G4Colour(1.0,1.0,0.0));
  // sc_hcalwin_log->SetVisAttributes(winVisAtt);
}

void G4SBSTargetBuilder::BuildTDISTarget(G4LogicalVolume *worldlog){

  if( fFlux ){ //Make a sphere to compute particle flux:
    G4Sphere *fsph = new G4Sphere( "fsph", 1.5*fTargLen/2.0, 1.5*fTargLen/2.0+cm, 0.0*deg, 360.*deg,
				   0.*deg, 150.*deg );
    G4LogicalVolume *fsph_log = new G4LogicalVolume( fsph, GetMaterial("Air"), "fsph_log" );
    new G4PVPlacement( 0, G4ThreeVector(0,0,0), fsph_log, "fsph_phys", worldlog, false, 0 );

    fsph_log->SetUserLimits(  new G4UserLimits(0.0, 0.0, 0.0, DBL_MAX, DBL_MAX) );
    
    G4String FluxSDname = "FLUX";
    G4String Fluxcollname = "FLUXHitsCollection";
    G4SBSCalSD *FluxSD = NULL;
    if( !( FluxSD = (G4SBSCalSD*) fDetCon->fSDman->FindSensitiveDetector(FluxSDname) ) ){
      G4cout << "Adding FLUX SD to SDman..." << G4endl;
      FluxSD = new G4SBSCalSD( FluxSDname, Fluxcollname );
      fDetCon->fSDman->AddNewDetector( FluxSD );
      (fDetCon->SDlist).insert( FluxSDname );
      fDetCon->SDtype[FluxSDname] = G4SBS::kCAL;

      (FluxSD->detmap).depth = 0;
    }
    fsph_log->SetSensitiveDetector( FluxSD );
  }
  
  //Desired minimum scattering angle for SBS = 5 deg (for SIDIS @10 deg):
  double sbs_scattering_angle_min = 5.0*deg;
  double sc_exitpipe_radius_inner = 48.0*mm;
  double sc_exitpipe_radius_outer = 50.0*mm;
  double sc_entrypipe_radius_inner = 31.75*mm;
  double sc_entrypipe_radius_outer = sc_entrypipe_radius_inner+0.12*mm;
  double sc_winthick = 0.38*mm;

  double zstart_sc = -1.5*m;
  double zend_sc   = 162.2*cm;

  double zpos_sc = (zstart_sc + zend_sc)/2.0;
  double dz_sc = (zend_sc - zstart_sc)/2.0 - sc_winthick;

  //Material definition was moved to ConstructMaterials();
  //--------- Glass target cell -------------------------------

  double wallthick = 0.01*mm;
  double capthick  = 0.01*mm;
  
  double sc_radius_inner = sc_exitpipe_radius_outer;
  double sc_radius_outer = sc_radius_inner + sc_winthick;

  G4Tubs *sc_wall = new G4Tubs("sc_tube", sc_radius_inner, sc_radius_outer, dz_sc, 0.*deg, 360.*deg );
  G4Tubs *sc_vacuum = new G4Tubs("sc_vac", 0.0, sc_radius_inner, dz_sc, 0.*deg, 360.*deg );

  G4Tubs *sc_cap_upstream = new G4Tubs("sc_cap_upstream", sc_entrypipe_radius_inner, sc_radius_outer, sc_winthick/2.0, 0.*deg, 360.*deg );
  G4Tubs *sc_cap_downstream = new G4Tubs("sc_cap_downstream", sc_exitpipe_radius_inner, sc_radius_outer, sc_winthick/2.0, 0.*deg, 360.*deg );
  
  G4LogicalVolume *sc_wall_log = new G4LogicalVolume( sc_wall, GetMaterial("Aluminum"), "sc_wall_log" );
  G4LogicalVolume *sc_vacuum_log = new G4LogicalVolume( sc_vacuum, GetMaterial("Vacuum"), "sc_vacuum_log" );
  G4LogicalVolume *sc_cap_upstream_log = new G4LogicalVolume( sc_cap_upstream, GetMaterial("Aluminum"), "sc_cap_upstream_log" );
  G4LogicalVolume *sc_cap_downstream_log = new G4LogicalVolume( sc_cap_downstream, GetMaterial("Aluminum"), "sc_cap_downstream_log" );
  
  G4Tubs *targ_tube = new G4Tubs("targ_tube", fTargDiameter/2.0-wallthick, fTargDiameter/2.0, fTargLen/2.0, 0.*deg, 360.*deg );
  G4Tubs *targ_cap = new G4Tubs("targ_cap", 0.0, fTargDiameter/2.0, capthick/2.0, 0.*deg, 360.*deg );

  G4LogicalVolume* targ_tube_log = new G4LogicalVolume(targ_tube, GetMaterial("Aluminum"),"targ_tube_log");
  G4LogicalVolume* targ_cap_log = new G4LogicalVolume(targ_cap, GetMaterial("Aluminum"),"targ_cap_log");

  fDetCon->InsertTargetVolume( sc_vacuum_log->GetName() );
  
  // gas
  G4Tubs *gas_tube = new G4Tubs("gas_tube", 0.0, fTargDiameter/2.0-wallthick,fTargLen/2.0, 0.*deg, 360.*deg );
  G4LogicalVolume* gas_tube_log = NULL;

  if( fTargType == G4SBS::kH2 || fTargType == G4SBS::kNeutTarg ){
    gas_tube_log = new G4LogicalVolume(gas_tube, GetMaterial("refH2"), "gas_tube_log");
  }
  if( fTargType == G4SBS::kD2 ){
    gas_tube_log = new G4LogicalVolume(gas_tube, GetMaterial("refD2"), "gas_tube_log");
  }
  if( fTargType == G4SBS::k3He ){
    gas_tube_log = new G4LogicalVolume(gas_tube, GetMaterial("pol3He"), "gas_tube_log");
  }

  fDetCon->InsertTargetVolume( gas_tube_log->GetName() );

  G4LogicalVolume *motherlog = worldlog;
  double target_zpos = 0.0;
  
  // if( fSchamFlag == 1 ){
  //   motherlog = sc_vacuum_log;
  //   target_zpos = -zpos_sc;
  // }
  
  //if( fTargType == G4SBS::kH2 || fTargType == G4SBS::k3He || fTargType == G4SBS::kNeutTarg ){
  new G4PVPlacement(0, G4ThreeVector(0.0, 0.0, target_zpos), targ_tube_log,
		    "targ_tube_phys", motherlog, false, 0);
  
  new G4PVPlacement(0, G4ThreeVector(0.0, 0.0, target_zpos+fTargLen/2.0+capthick/2.0), targ_cap_log,
		    "targ_cap_phys1", motherlog, false, 0);
  new G4PVPlacement(0, G4ThreeVector(0.0, 0.0, target_zpos-fTargLen/2.0-capthick/2.0), targ_cap_log,
		    "targ_cap_phys2", motherlog, false, 1);
  
  assert(gas_tube_log);
  new G4PVPlacement(0, G4ThreeVector(0.0, 0.0, target_zpos), gas_tube_log,
		    "gas_tube_phys", motherlog, false, 0);
  
  BuildTPC(motherlog, target_zpos+5.0*cm);//TPC will always be 10cm longer than target.
  
  // //Place scattering chamber:
  // if( fSchamFlag == 1 ){
  //   new G4PVPlacement( 0, G4ThreeVector(0.0, 0.0, zpos_sc), sc_wall_log, "sc_wall_phys", worldlog, false, 0 );
  //   new G4PVPlacement( 0, G4ThreeVector(0.0, 0.0, zpos_sc-dz_sc-sc_winthick/2.0), sc_cap_upstream_log, "sc_cap_upstream_phys", worldlog, false, 0 );
  //   new G4PVPlacement( 0, G4ThreeVector(0.0, 0.0, zpos_sc+dz_sc+sc_winthick/2.0), sc_cap_downstream_log, "sc_cap_downstream_phys", worldlog, false, 0 );
  //   new G4PVPlacement( 0, G4ThreeVector(0.0, 0.0, zpos_sc), sc_vacuum_log, "sc_vacuum_phys", worldlog, false, 0 );
  // }
  
  // sc_vacuum_log->SetVisAttributes( G4VisAttributes::Invisible );

  //Visualization attributes:
  //ScatteringChamber_log->SetVisAttributes( G4VisAttributes::Invisible );

  G4VisAttributes *sc_wall_visatt = new G4VisAttributes( G4Colour( 0.5, 0.5, 0.5 ) );
  sc_wall_visatt->SetForceWireframe(true);

  // sc_wall_log->SetVisAttributes( sc_wall_visatt );
  sc_cap_upstream_log->SetVisAttributes( sc_wall_visatt );
  sc_cap_upstream_log->SetVisAttributes( sc_wall_visatt );

  // G4VisAttributes *sc_exit_pipe_visatt = new G4VisAttributes( G4Colour( 0.5, 0.5, 0.5 ) );
  // sc_exit_pipe_log->SetVisAttributes( sc_exit_pipe_visatt );

  // G4VisAttributes *sc_win_visatt = new G4VisAttributes( G4Colour( 0.8, 0.8, 0.0 ) );
  // sc_window_sbs_log->SetVisAttributes( sc_win_visatt );
  // //sc_window_bb_log->SetVisAttributes( sc_win_visatt );

  G4VisAttributes *tgt_cell_visatt = new G4VisAttributes( G4Colour( 1.0, 1.0, 1.0 ) );
  tgt_cell_visatt->SetForceWireframe(true);

  targ_cap_log->SetVisAttributes( tgt_cell_visatt );
  targ_tube_log->SetVisAttributes( tgt_cell_visatt );

  G4VisAttributes *tgt_gas_visatt = new G4VisAttributes( G4Colour( 0.0, 1.0, 1.0 ) );
  gas_tube_log->SetVisAttributes( tgt_gas_visatt );

}

void G4SBSTargetBuilder::BuildTPC(G4LogicalVolume *motherlog, G4double z_pos){
  // oversimplistic TPC
  G4Tubs* TPCmother_solid = 
    new G4Tubs("TPCmother_solid", 10.0*cm/2.0, 30.*cm/2.0, (fTargLen+10.0*cm)/2.0, 0.*deg, 360.*deg );
  G4Tubs* TPCinnerwall_solid = 
    new G4Tubs("TPCinnerwall_solid", 10.0*cm/2.0, 10.*cm/2.0+0.002*mm, (fTargLen+10.0*cm)/2.0, 0.*deg, 360.*deg );
  G4Tubs* TPCouterwall_solid = 
    new G4Tubs("TPCouterwall_solid", 30.0*cm/2.0-0.002*mm, 30.*cm/2.0, (fTargLen+10.0*cm)/2.0, 0.*deg, 360.*deg );
  G4Tubs* TPCgas_solid = 
    new G4Tubs("TPCgas_solid", 10.*cm/2.0+0.002*mm, 30.0*cm/2.0-0.002*mm, (fTargLen+10.0*cm)/2.0, 0.*deg, 360.*deg );
  
  G4LogicalVolume* TPCmother_log = 
    new G4LogicalVolume(TPCmother_solid, GetMaterial("Air"),"TPCmother_log");
  G4LogicalVolume* TPCinnerwall_log = 
    new G4LogicalVolume(TPCinnerwall_solid, GetMaterial("Kapton"),"TPCinnerwall_log");
  G4LogicalVolume* TPCouterwall_log = 
    new G4LogicalVolume(TPCouterwall_solid, GetMaterial("Kapton"),"TPCouterwall_log");
  G4LogicalVolume* TPCgas_log = 
    new G4LogicalVolume(TPCgas_solid, GetMaterial("ref4He"),"TPCouterwall_log");
  
  new G4PVPlacement(0, G4ThreeVector(0.0, 0.0, z_pos), TPCmother_log,
		    "TPCmother_phys", motherlog, false, 0);
  new G4PVPlacement(0, G4ThreeVector(0.0, 0.0, 0.0), TPCinnerwall_log,
		    "TPCinnerwall_phys", TPCmother_log, false, 0);
  new G4PVPlacement(0, G4ThreeVector(0.0, 0.0, 0.0), TPCouterwall_log,
		    "TPCouterwall_phys", TPCmother_log, false, 0);
  new G4PVPlacement(0, G4ThreeVector(0.0, 0.0, 0.0), TPCgas_log,
		    "TPCgas_phys", TPCmother_log, false, 0);
  
  // sensitize gas
  
  // Visualization attributes
  TPCmother_log->SetVisAttributes( G4VisAttributes::Invisible );
  
  G4VisAttributes *tpcwalls_visatt = new G4VisAttributes( G4Colour( 1.0, 1.0, 1.0 ) );
  tpcwalls_visatt->SetForceWireframe(true);
  TPCinnerwall_log->SetVisAttributes( tpcwalls_visatt );
  TPCouterwall_log->SetVisAttributes( tpcwalls_visatt );
  
  G4VisAttributes *tpcgas_visatt = new G4VisAttributes( G4Colour( 1.0, 1.0, 0.0, 0.5 ) );
  TPCgas_log->SetVisAttributes( tpcgas_visatt );
  
}

void G4SBSTargetBuilder::BuildGasTarget(G4LogicalVolume *worldlog){

  if( fFlux ){ //Make a sphere to compute particle flux:
    G4Sphere *fsph = new G4Sphere( "fsph", 1.5*fTargLen/2.0, 1.5*fTargLen/2.0+cm, 0.0*deg, 360.*deg,
				   0.*deg, 150.*deg );
    G4LogicalVolume *fsph_log = new G4LogicalVolume( fsph, GetMaterial("Air"), "fsph_log" );
    new G4PVPlacement( 0, G4ThreeVector(0,0,0), fsph_log, "fsph_phys", worldlog, false, 0 );

    fsph_log->SetUserLimits(  new G4UserLimits(0.0, 0.0, 0.0, DBL_MAX, DBL_MAX) );
    
    G4String FluxSDname = "FLUX";
    G4String Fluxcollname = "FLUXHitsCollection";
    G4SBSCalSD *FluxSD = NULL;
    if( !( FluxSD = (G4SBSCalSD*) fDetCon->fSDman->FindSensitiveDetector(FluxSDname) ) ){
      G4cout << "Adding FLUX SD to SDman..." << G4endl;
      FluxSD = new G4SBSCalSD( FluxSDname, Fluxcollname );
      fDetCon->fSDman->AddNewDetector( FluxSD );
      (fDetCon->SDlist).insert( FluxSDname );
      fDetCon->SDtype[FluxSDname] = G4SBS::kCAL;

      (FluxSD->detmap).depth = 0;
    }
    fsph_log->SetSensitiveDetector( FluxSD );
  }
  
  //Desired minimum scattering angle for SBS = 5 deg (for SIDIS @10 deg):
  double sbs_scattering_angle_min = 5.0*deg;
  double sc_exitpipe_radius_inner = 48.0*mm;
  double sc_exitpipe_radius_outer = 50.0*mm;
  double sc_entrypipe_radius_inner = 31.75*mm;
  double sc_entrypipe_radius_outer = sc_entrypipe_radius_inner+0.12*mm;
  double sc_winthick = 0.38*mm;

  double zstart_sc = -1.5*m;
  double zend_sc   = 162.2*cm;

  double zpos_sc = (zstart_sc + zend_sc)/2.0;
  double dz_sc = (zend_sc - zstart_sc)/2.0 - sc_winthick;

  //Material definition was moved to ConstructMaterials();
  //--------- Glass target cell -------------------------------

  double wallthick = 1.61*mm;
  double capthick  = 0.126*mm;
  
  double sc_radius_inner = sc_exitpipe_radius_outer;
  double sc_radius_outer = sc_radius_inner + sc_winthick;

  G4Tubs *sc_wall = new G4Tubs("sc_tube", sc_radius_inner, sc_radius_outer, dz_sc, 0.*deg, 360.*deg );
  G4Tubs *sc_vacuum = new G4Tubs("sc_vac", 0.0, sc_radius_inner, dz_sc, 0.*deg, 360.*deg );

  G4Tubs *sc_cap_upstream = new G4Tubs("sc_cap_upstream", sc_entrypipe_radius_inner, sc_radius_outer, sc_winthick/2.0, 0.*deg, 360.*deg );
  G4Tubs *sc_cap_downstream = new G4Tubs("sc_cap_downstream", sc_exitpipe_radius_inner, sc_radius_outer, sc_winthick/2.0, 0.*deg, 360.*deg );
  
  G4LogicalVolume *sc_wall_log = new G4LogicalVolume( sc_wall, GetMaterial("Aluminum"), "sc_wall_log" );
  G4LogicalVolume *sc_vacuum_log = new G4LogicalVolume( sc_vacuum, GetMaterial("Vacuum"), "sc_vacuum_log" );
  G4LogicalVolume *sc_cap_upstream_log = new G4LogicalVolume( sc_cap_upstream, GetMaterial("Aluminum"), "sc_cap_upstream_log" );
  G4LogicalVolume *sc_cap_downstream_log = new G4LogicalVolume( sc_cap_downstream, GetMaterial("Aluminum"), "sc_cap_downstream_log" );
  
  G4Tubs *targ_tube = new G4Tubs("targ_tube", fTargDiameter/2.0-wallthick, fTargDiameter/2.0, fTargLen/2.0, 0.*deg, 360.*deg );
  G4Tubs *targ_cap = new G4Tubs("targ_cap", 0.0, fTargDiameter/2.0, capthick/2.0, 0.*deg, 360.*deg );

  G4LogicalVolume* targ_tube_log = new G4LogicalVolume(targ_tube, GetMaterial("GE180"),"targ_tube_log");
  G4LogicalVolume* targ_cap_log = new G4LogicalVolume(targ_cap, GetMaterial("GE180"),"targ_cap_log");

  // gas
  G4Tubs *gas_tube = new G4Tubs("gas_tube", 0.0, fTargDiameter/2.0-wallthick,fTargLen/2.0, 0.*deg, 360.*deg );
  G4LogicalVolume* gas_tube_log = NULL;


  if( fTargType == G4SBS::kH2 || fTargType == G4SBS::kNeutTarg ){
    gas_tube_log = new G4LogicalVolume(gas_tube, GetMaterial("refH2"), "gas_tube_log");
  }
  if( fTargType == G4SBS::kD2 ){
    gas_tube_log = new G4LogicalVolume(gas_tube, GetMaterial("refD2"), "gas_tube_log");
  }
  if( fTargType == G4SBS::k3He ){
    gas_tube_log = new G4LogicalVolume(gas_tube, GetMaterial("pol3He"), "gas_tube_log");
  }

  //Insert the target volumes into the list of "Target volumes" for storing track history (see G4SBSTrackInformation and G4SBSTrackingAction)
  fDetCon->InsertTargetVolume( targ_cap_log->GetName() );
  fDetCon->InsertTargetVolume( targ_tube_log->GetName() );
  fDetCon->InsertTargetVolume( gas_tube_log->GetName() );
  
  G4LogicalVolume *motherlog = worldlog;
  double target_zpos = 0.0;
  
  if( fSchamFlag == 1 ){
    motherlog = sc_vacuum_log;
    target_zpos = -zpos_sc;
  }

  //We don't always want to place the gas target; we might want some foil or optics targets in air:
  if( fTargType == G4SBS::kH2 || fTargType == G4SBS::k3He || fTargType == G4SBS::kNeutTarg || fTargType == G4SBS::kD2 ){
    new G4PVPlacement(0, G4ThreeVector(0.0, 0.0, target_zpos), targ_tube_log,
		      "targ_tube_phys", motherlog, false, 0);
  
    new G4PVPlacement(0, G4ThreeVector(0.0, 0.0, target_zpos+fTargLen/2.0+capthick/2.0), targ_cap_log,
		      "targ_cap_phys1", motherlog, false, 0);
    new G4PVPlacement(0, G4ThreeVector(0.0, 0.0, target_zpos-fTargLen/2.0-capthick/2.0), targ_cap_log,
		      "targ_cap_phys2", motherlog, false, 1);
  
    assert(gas_tube_log);
    new G4PVPlacement(0, G4ThreeVector(0.0, 0.0, target_zpos), gas_tube_log,
		      "gas_tube_phys", motherlog, false, 0);
  } else if( fTargType == G4SBS::kCfoil ){ //single carbon foil
    BuildCfoil( motherlog, 0, G4ThreeVector(0,0,target_zpos) );
  } else if (fTargType == G4SBS::kOptics ){ //multi-foil (optics) target:
    BuildOpticsTarget( motherlog, 0, G4ThreeVector(0,0,target_zpos) );
  }

  //Never mind.
  //Place scattering chamber:
  if( fSchamFlag == 1 ){
    new G4PVPlacement( 0, G4ThreeVector(0.0, 0.0, zpos_sc), sc_wall_log, "sc_wall_phys", worldlog, false, 0 );
    new G4PVPlacement( 0, G4ThreeVector(0.0, 0.0, zpos_sc-dz_sc-sc_winthick/2.0), sc_cap_upstream_log, "sc_cap_upstream_phys", worldlog, false, 0 );
    new G4PVPlacement( 0, G4ThreeVector(0.0, 0.0, zpos_sc+dz_sc+sc_winthick/2.0), sc_cap_downstream_log, "sc_cap_downstream_phys", worldlog, false, 0 );
    new G4PVPlacement( 0, G4ThreeVector(0.0, 0.0, zpos_sc), sc_vacuum_log, "sc_vacuum_phys", worldlog, false, 0 );
  }
  
  sc_vacuum_log->SetVisAttributes( G4VisAttributes::Invisible );

  //Visualization attributes:
  //ScatteringChamber_log->SetVisAttributes( G4VisAttributes::Invisible );

  G4VisAttributes *sc_wall_visatt = new G4VisAttributes( G4Colour( 0.5, 0.5, 0.5 ) );
  sc_wall_visatt->SetForceWireframe(true);

  sc_wall_log->SetVisAttributes( sc_wall_visatt );
  sc_cap_upstream_log->SetVisAttributes( sc_wall_visatt );
  sc_cap_upstream_log->SetVisAttributes( sc_wall_visatt );

  // G4VisAttributes *sc_exit_pipe_visatt = new G4VisAttributes( G4Colour( 0.5, 0.5, 0.5 ) );
  // sc_exit_pipe_log->SetVisAttributes( sc_exit_pipe_visatt );

  // G4VisAttributes *sc_win_visatt = new G4VisAttributes( G4Colour( 0.8, 0.8, 0.0 ) );
  // sc_window_sbs_log->SetVisAttributes( sc_win_visatt );
  // //sc_window_bb_log->SetVisAttributes( sc_win_visatt );

  G4VisAttributes *tgt_cell_visatt = new G4VisAttributes( G4Colour( 1.0, 1.0, 1.0 ) );
  tgt_cell_visatt->SetForceWireframe(true);

  targ_cap_log->SetVisAttributes( tgt_cell_visatt );
  targ_tube_log->SetVisAttributes( tgt_cell_visatt );

  G4VisAttributes *tgt_gas_visatt = new G4VisAttributes( G4Colour( 0.0, 1.0, 1.0 ) );
  gas_tube_log->SetVisAttributes( tgt_gas_visatt );

}

void G4SBSTargetBuilder::SetNtargetFoils(G4int nfoil){
  fNtargetFoils = nfoil;
  fFoilThick.clear();
  fFoilThick.resize(nfoil);
  fFoilZpos.clear();
  fFoilZpos.resize(nfoil);
}

void G4SBSTargetBuilder::SetFoilThick(G4int ifoil, G4double foilthick ){
  if( ifoil >= 0 && ifoil < fNtargetFoils ){
    fFoilThick[ifoil] = foilthick;
  }
}

void G4SBSTargetBuilder::SetFoilZpos(G4int ifoil, G4double foilz ){
  if( ifoil >= 0 && ifoil < fNtargetFoils ){
    fFoilZpos[ifoil] = foilz;
  }
}

void G4SBSTargetBuilder::BuildToyScatCham( G4LogicalVolume *motherlog ){
  //This is going to be hard-coded for now: and we will build generic, somewhat fictitious extensions of the beamline vacuum:

  G4double sc_height = 1.0*m; //for lack of better information, let's make this 1 meter tall
  G4double sc_diam_inner = 41.0*2.54*cm; //1.04 m
  G4double sc_diam_outer = sc_diam_inner + 2.0*2.54*cm; //assume walls are 2-inch thick, let's use Al for material (but who really cares?)
  //assume vacuum flange cutouts start at 5 deg on either side of the beamline (we could adjust this later)
  G4double thetamin_proton = 100.0*deg; 
  G4double thetamax_proton = 80.0*deg; 
  G4double thetamin_electron = -30.0*deg;
  G4double thetamax_electron = 115.0*deg;

  //somewhat arbitrarily, make the scattering chamber wall cuts 15 inches high:
  G4double sc_wallcut_height_electron = 15.0*2.54*cm;
  G4double sc_wallcut_height_proton = 15.0*2.54*cm;
  
  //scattering chamber vacuum: 
  G4Tubs *sc_vacuum_tube = new G4Tubs( "sc_vacuum_tube", 0.0, sc_diam_outer/2.0, sc_height/2.0, 0.0, twopi );

  G4LogicalVolume *sc_vacuum_tube_log = new G4LogicalVolume( sc_vacuum_tube, GetMaterial("Vacuum"), "sc_vacuum_tube_log" );

  //scattering chamber walls:
  G4Tubs *sc_wall_tube = new G4Tubs( "sc_wall_tube", sc_diam_inner/2.0, sc_diam_outer/2.0, sc_height/2.0, 0.0, twopi );

  //We will need to cut holes in the walls of the toy scattering chamber for beam entry and exit ports and for thin exit windows for scattered particles:
  G4RotationMatrix *rot_temp = new G4RotationMatrix;

  rot_temp->rotateX(-90.0*deg);

  G4Tubs *sc_wall_beampipe_cut = new G4Tubs( "sc_wall_beampipe_cut", 0.0, 55.0*mm, sc_diam_outer/2.0+cm, 0.0, twopi );

  //Cut beam entry and exit ports out of scattering chamber wall:
  G4SubtractionSolid *sc_wall_cutbeampipe = new G4SubtractionSolid( "sc_wall_cutbeampipe", sc_wall_tube, sc_wall_beampipe_cut, rot_temp, G4ThreeVector(0,0,0) );

  G4Tubs *sc_wallcut_electron = new G4Tubs( "sc_wallcut_electron", sc_diam_inner/2.0-cm, sc_diam_outer/2.0+cm, sc_wallcut_height_electron/2.0, thetamin_electron, thetamax_electron );

  G4Tubs *sc_wallcut_proton = new G4Tubs( "sc_wallcut_proton", sc_diam_inner/2.0-cm, sc_diam_outer/2.0+cm, sc_wallcut_height_proton/2.0, thetamin_proton, thetamax_proton );

  G4SubtractionSolid *sc_wall_cutelectron = new G4SubtractionSolid( "sc_wall_cutelectron", sc_wall_cutbeampipe, sc_wallcut_electron, 0, G4ThreeVector(0,0,0) );

  G4SubtractionSolid *sc_wall_cutproton = new G4SubtractionSolid( "sc_wall_cutproton", sc_wall_cutelectron, sc_wallcut_proton, 0, G4ThreeVector(0,0,0) );
  
  G4LogicalVolume *sc_wall_log = new G4LogicalVolume( sc_wall_cutproton, GetMaterial("Aluminum"), "sc_wall_log" );


  new G4PVPlacement( 0, G4ThreeVector(0,0,0), sc_wall_log, "sc_wall_phys", sc_vacuum_tube_log, false, 0 );

  
  G4RotationMatrix *rot_sc = new G4RotationMatrix;
  rot_sc->rotateX(-90.0*deg);

  sc_vacuum_tube_log->SetVisAttributes(G4VisAttributes::Invisible);
  
  new G4PVPlacement( rot_sc, G4ThreeVector(0,0,0), sc_vacuum_tube_log, "scatcham_phys", motherlog, false, 0 );

  //G4Tubs *earm_window = 
  
}

void G4SBSTargetBuilder::BuildRadiator(G4LogicalVolume *motherlog, G4RotationMatrix *rot, G4ThreeVector pos){
  G4double radthick = GetMaterial("Copper")->GetRadlen()*fRadThick;

  //  G4cout << "Radiation length = " << fRadThick*100.0 << " % = " << radthick/mm << " mm" << G4endl;
  
  G4Box *radbox = new G4Box("radbox", fTargDiameter/2.0, fTargDiameter/2.0, radthick/2.0 );

  G4LogicalVolume *radlog = new G4LogicalVolume( radbox, GetMaterial("Copper"), "radlog" );

  new G4PVPlacement( rot, pos, radlog, "radphys", motherlog, false, 0 );
}

void G4SBSTargetBuilder::BuildGEnTarget(G4LogicalVolume *motherLog){
   // Polarized 3He target for GEn
   // - geometry based on drawings from Bert Metzger and Gordon Cates  

   // glass cell
   // NOTE: The polarized 3He is built inside the GlassCell function!
   //       This is because the glass cell is used as the mother volume of the 3He  
   BuildGEnTarget_GlassCell(motherLog);

   // Cu end windows for the target cell 
   BuildGEnTarget_EndWindows(motherLog);

   // cylinder of polarized 3He
   BuildGEnTarget_PolarizedHe3(motherLog);

   // helmholtz coils
   int config = fDetCon->GetGEnTargetHelmholtzConfig();
   double Q2=0; 
   if(config==G4SBS::kSBS_GEN_146)  Q2 = 1.46; 
   if(config==G4SBS::kSBS_GEN_368)  Q2 = 3.68; 
   if(config==G4SBS::kSBS_GEN_677)  Q2 = 6.77; 
   if(config==G4SBS::kSBS_GEN_1018) Q2 = 10.18;
   G4cout << "[G4SBSTargetBuilder::BuildGEnTarget]: Using config for Q2 = " << Q2 << " (GeV/c)^2" << G4endl; 

   BuildGEnTarget_HelmholtzCoils(config,"maj",motherLog);
   BuildGEnTarget_HelmholtzCoils(config,"rfy",motherLog);
   BuildGEnTarget_HelmholtzCoils(config,"min",motherLog);

   // magnetic shield
   // config = kSBS_GEN_new; // for when the shield design is finalized  
   BuildGEnTarget_Shield(config,motherLog);

   // target ladder 
   BuildGEnTarget_LadderPlate(motherLog);

   // pickup coils 
   BuildGEnTarget_PickupCoils(motherLog);

}

void G4SBSTargetBuilder::BuildGEnTarget_GlassCell(G4LogicalVolume *motherLog){
   // Glass cell for polarized 3He
   // - drawing number: internal from G. Cates (May 2020) 

   G4double glassWall = 1.0*mm; // estimate 
   G4double tubeLength = 571.7*mm; // 579.0*mm;  

   // pumping chamber 
   partParameters_t pumpCh; 
   pumpCh.name = "pumpingChamber"; pumpCh.shape = "sphere"; 
   pumpCh.r_tor = 0.0*mm; pumpCh.r_max = 54.0*mm; pumpCh.r_min = pumpCh.r_max - glassWall; pumpCh.length = 0.0*mm;
   pumpCh.x_len = 0.0*mm; pumpCh.y_len = 0.0*mm; pumpCh.z_len = 0.0*mm;
   pumpCh.startTheta = 0.0*deg; pumpCh.dTheta = 180.0*deg;
   pumpCh.startPhi = 0.0*deg; pumpCh.dPhi = 360.0*deg;
   pumpCh.x = 0.0*mm; pumpCh.y = -330.2*mm; pumpCh.z = 0.0*mm;
   pumpCh.rx = 0.0*deg; pumpCh.ry = 0.0*deg; pumpCh.rz = 0.0*deg;

   G4Sphere *pumpChamberShape = new G4Sphere(pumpCh.name,
	                                     pumpCh.r_min     ,pumpCh.r_max,
	                                     pumpCh.startPhi  ,pumpCh.dPhi,
	                                     pumpCh.startTheta,pumpCh.dTheta);

   G4ThreeVector P_pc = G4ThreeVector(pumpCh.x,pumpCh.y,pumpCh.z);
   G4RotationMatrix *rm_pc = new G4RotationMatrix();
   rm_pc->rotateX(pumpCh.rx); rm_pc->rotateY(pumpCh.ry); rm_pc->rotateZ(pumpCh.rz);

   // target chamber 
   partParameters_t tgtCh;
   tgtCh.name = "targetChamber"; tgtCh.shape = "tube"; 
   tgtCh.r_tor = 0.0*mm; tgtCh.r_max = 10.5*mm; tgtCh.r_min = tgtCh.r_max - glassWall; tgtCh.length = tubeLength;
   tgtCh.x_len = 0.0*mm; tgtCh.y_len = 0.0*mm; tgtCh.z_len = 0.0*mm;
   tgtCh.startTheta = 0.0*deg; tgtCh.dTheta = 0.0*deg;
   tgtCh.startPhi = 0.0*deg; tgtCh.dPhi = 360.0*deg;
   tgtCh.x = 0.0*mm; tgtCh.y = 0.0*mm; tgtCh.z = 0.0*mm;
   tgtCh.rx = 0.0*deg; tgtCh.ry = 0.0*deg; tgtCh.rz = 0.0*deg;

   G4Tubs *targetChamberShape = new G4Tubs(tgtCh.name,
	                                   tgtCh.r_min    ,tgtCh.r_max,
	                                   tgtCh.length/2.,
	                                   tgtCh.startPhi ,tgtCh.dPhi);

   G4ThreeVector P_tc = G4ThreeVector(tgtCh.x,tgtCh.y,tgtCh.z);
   G4RotationMatrix *rm_tc = new G4RotationMatrix();
   rm_tc->rotateX(tgtCh.rx); rm_tc->rotateY(tgtCh.ry); rm_tc->rotateZ(tgtCh.rz);

   // transfer tube elbow
   // ---- downstream 
   partParameters_t tted;
   tted.name = "transTubeEl_dn"; tted.shape = "torus"; 
   tted.r_tor = 9.0*mm; tted.r_max = 4.5*mm; tted.r_min = tted.r_max - glassWall; tted.length = 0.0*mm;
   tted.x_len = 0.0*mm; tted.y_len = 0.0*mm; tted.z_len = 0.0*mm;
   tted.startTheta = 0.0*deg; tted.dTheta = 0.0*deg;
   tted.startPhi = 0.0*deg; tted.dPhi = 90.0*deg;
   tted.x = 0.0*mm; tted.y = -33.5*mm; tted.z = 250.0*mm;
   tted.rx = 180.0*deg; tted.ry = -90.0*deg; tted.rz = 0.0*deg;

   G4Torus *transTubeElDnShape = new G4Torus(tted.name,
                                             tted.r_min   ,tted.r_max,tted.r_tor,
                                             tted.startPhi,tted.dPhi);

   G4ThreeVector P_tted = G4ThreeVector(tted.x,tted.y,tted.z);
   G4RotationMatrix *rm_tted = new G4RotationMatrix();
   rm_tted->rotateX(tted.rx); rm_tted->rotateY(tted.ry); rm_tted->rotateZ(tted.rz);

   // ---- upstream
   partParameters_t tteu; 
   tteu.name = "transTubeEl_up"; tteu.shape = "torus"; 
   tteu.r_tor = 9.0*mm; tteu.r_max = 4.5*mm; tteu.r_min = tteu.r_max - glassWall; tteu.length = 0.0*mm;
   tteu.x_len = 0.0*mm; tteu.y_len = 0.0*mm; tteu.z_len = 0.0*mm;
   tteu.startTheta = 0.0*deg; tteu.dTheta = 0.0*deg;
   tteu.startPhi = 0.0*deg; tteu.dPhi = 90.0*deg;
   tteu.x = 0.0*mm; tteu.y = -33.5*mm; tteu.z = -250.0*mm;
   tteu.rx = 180.0*deg; tteu.ry = 90.0*deg; tteu.rz = 0.0*deg;

   G4Torus *transTubeElUpShape = new G4Torus(tteu.name,
                                             tteu.r_min   ,tteu.r_max,tteu.r_tor,
                                             tteu.startPhi,tteu.dPhi);

   G4ThreeVector P_tteu = G4ThreeVector(tteu.x,tteu.y,tteu.z);
   G4RotationMatrix *rm_tteu = new G4RotationMatrix();
   rm_tteu->rotateX(tteu.rx); rm_tteu->rotateY(tteu.ry); rm_tteu->rotateZ(tteu.rz);

   // transfer tube elbow, lower 
   // ---- downstream
   partParameters_t ttedl; 
   ttedl.name = "transTubeElLo_dn"; ttedl.shape = "torus"; 
   ttedl.r_tor = 9.0*mm; ttedl.r_max = 4.5*mm; ttedl.r_min = ttedl.r_max - glassWall; ttedl.length = 0.0*mm;
   ttedl.x_len = 0.0*mm; ttedl.y_len = 0.0*mm; ttedl.z_len = 0.0*mm;
   ttedl.startTheta = 0.0*deg; ttedl.dTheta = 0.0*deg;
   ttedl.startPhi = 0.0*deg; ttedl.dPhi = 90.0*deg;
   ttedl.x = 0.0*mm; ttedl.y = -51.5*mm; ttedl.z = 34.5*mm;
   ttedl.rx = 0.0*deg; ttedl.ry = 270.0*deg; ttedl.rz = 0.0*deg;

   G4Torus *transTubeElDnLoShape = new G4Torus(ttedl.name,
                                               ttedl.r_min   ,ttedl.r_max,ttedl.r_tor,
                                               ttedl.startPhi,ttedl.dPhi);

   G4ThreeVector P_ttedl = G4ThreeVector(ttedl.x,ttedl.y,ttedl.z);
   G4RotationMatrix *rm_ttedl = new G4RotationMatrix();
   rm_ttedl->rotateX(ttedl.rx); rm_ttedl->rotateY(ttedl.ry); rm_ttedl->rotateZ(ttedl.rz);

   // ---- upstream
   partParameters_t tteul; 
   tteul.name = "transTubeElLo_up"; tteul.shape = "torus"; 
   tteul.r_tor = 9.0*mm; tteul.r_max = 4.5*mm; tteul.r_min = tteul.r_max - glassWall; tteul.length = 0.0*mm;
   tteul.x_len = 0.0*mm; tteul.y_len = 0.0*mm; tteul.z_len = 0.0*mm;
   tteul.startTheta = 0.0*deg; tteul.dTheta = 0.0*deg;
   tteul.startPhi = 0.0*deg; tteul.dPhi = 90.0*deg;
   tteul.x = 0.0*mm; tteul.y = -51.5*mm; tteul.z = -34.5*mm;
   tteul.rx = 0.0*deg; tteul.ry = 90.0*deg; tteul.rz = 0.0*deg;

   G4Torus *transTubeElUpLoShape = new G4Torus(tteul.name,
	                                       tteul.r_min   ,tteul.r_max,tteul.r_tor,
	                                       tteul.startPhi,tteul.dPhi);

   G4ThreeVector P_tteul = G4ThreeVector(tteul.x,tteul.y,tteul.z);
   G4RotationMatrix *rm_tteul = new G4RotationMatrix();
   rm_tteul->rotateX(tteul.rx); rm_tteul->rotateY(tteul.ry); rm_tteul->rotateZ(tteul.rz); 
   
   // transfer tube sphere 
   partParameters_t tts;
   tts.name = "transTubeSphere"; tts.shape = "sphere"; 
   tts.r_tor = 0.0*mm; tts.r_max = 13.9*mm; tts.r_min = tts.r_max - glassWall; tts.length = 0.0*mm;
   tts.x_len = 0.0*mm; tts.y_len = 0.0*mm; tts.z_len = 0.0*mm;
   tts.startTheta = 0.0*deg; tts.dTheta = 180.0*deg;
   tts.startPhi = 0.0*deg; tts.dPhi = 360.0*deg;
   tts.x = 0.0*mm; tts.y = -83.3*mm; tts.z = 25.4*mm;
   tts.rx = 0.0*deg; tts.ry = 0.0*deg; tts.rz = 0.0*deg;

   G4Sphere *transTubeSphere = new G4Sphere(tts.name,
	                                    tts.r_min     ,tts.r_max,
	                                    tts.startPhi  ,tts.dPhi,
	                                    tts.startTheta,tts.dTheta);

   G4ThreeVector P_tts = G4ThreeVector(tts.x,tts.y,tts.z);
   G4RotationMatrix *rm_tts = new G4RotationMatrix();
   rm_tts->rotateX(tts.rx); rm_tts->rotateY(tts.ry); rm_tts->rotateZ(tts.rz);

   // transfer tubes along z 
   // ---- upstream 
   partParameters_t ttuz; 
   ttuz.name = "transTubeZ_up"; ttuz.shape = "tube"; 
   ttuz.r_tor = 0.0*mm; ttuz.r_max = 4.5*mm; ttuz.r_min = ttuz.r_max - glassWall; ttuz.length = 216.0*mm;
   ttuz.x_len = 0.0*mm; ttuz.y_len = 0.0*mm; ttuz.z_len = 0.0*mm;
   ttuz.startTheta = 0.0*deg; ttuz.dTheta = 0.0*deg;
   ttuz.startPhi = 0.0*deg; ttuz.dPhi = 360.0*deg;
   ttuz.x = 0.0*mm; ttuz.y = -42.5*mm; ttuz.z = -142.0*mm;
   ttuz.rx = 0.0*deg; ttuz.ry = 0.0*deg; ttuz.rz = 0.0*deg;
   
   G4Tubs *transTubeUpZShape = new G4Tubs(ttuz.name,
                                          ttuz.r_min    ,ttuz.r_max,
                                          ttuz.length/2.,
                                          ttuz.startPhi ,ttuz.dPhi);

   G4ThreeVector P_ttuz = G4ThreeVector(ttuz.x,ttuz.y,ttuz.z);
   G4RotationMatrix *rm_ttuz = new G4RotationMatrix();
   rm_ttuz->rotateX(ttuz.rx); rm_ttuz->rotateY(ttuz.ry); rm_ttuz->rotateZ(ttuz.rz);

   // ---- downstream 
   partParameters_t ttdz; 
   ttdz.name = "transTubeZ_dn"; ttdz.shape = "tube"; 
   ttdz.r_tor = 0.0*mm; ttdz.r_max = 4.5*mm; ttdz.r_min = ttdz.r_max - glassWall; ttdz.length = 216.0*mm;
   ttdz.x_len = 0.0*mm; ttdz.y_len = 0.0*mm; ttdz.z_len = 0.0*mm;
   ttdz.startTheta = 0.0*deg; ttdz.dTheta = 0.0*deg;
   ttdz.startPhi = 0.0*deg; ttdz.dPhi = 360.0*deg;
   ttdz.x = 0.0*mm; ttdz.y = -42.5*mm; ttdz.z = 142.0*mm;
   ttdz.rx = 0.0*deg; ttdz.ry = 0.0*deg; ttdz.rz = 0.0*deg;

   G4Tubs *transTubeDnZShape = new G4Tubs(ttdz.name,
	                                  ttdz.r_min    ,ttdz.r_max,
	                                  ttdz.length/2.,
	                                  ttdz.startPhi ,ttdz.dPhi);

   G4ThreeVector P_ttdz = G4ThreeVector(ttdz.x,ttdz.y,ttdz.z);
   G4RotationMatrix *rm_ttdz = new G4RotationMatrix();
   rm_ttdz->rotateX(ttdz.rx); rm_ttdz->rotateY(ttdz.ry); rm_ttdz->rotateZ(ttdz.rz);

   // transfer tubes along y 
   // --- upstream 
   partParameters_t ttuy; 
   ttuy.name = "transTubeY_up"; ttuy.shape = "tube"; 
   ttuy.r_tor = 0.0*mm; ttuy.r_max = 4.5*mm; ttuy.r_min = ttuy.r_max - glassWall; ttuy.length = 233.0*mm;
   ttuy.x_len = 0.0*mm; ttuy.y_len = 0.0*mm; ttuy.z_len = 0.0*mm;
   ttuy.startTheta = 0.0*deg; ttuy.dTheta = 0.0*deg;
   ttuy.startPhi = 0.0*deg; ttuy.dPhi = 360.0*deg;
   ttuy.x = 0.0*mm; ttuy.y = -168.0*mm; ttuy.z = -25.4*mm;
   ttuy.rx = 90.0*deg; ttuy.ry = 0.0*deg; ttuy.rz = 0.0*deg;

   G4Tubs *transTubeUpYShape = new G4Tubs(ttuy.name,
	                                  ttuy.r_min    ,ttuy.r_max,
	                                  ttuy.length/2.,
	                                  ttuy.startPhi ,ttuy.dPhi);

   G4ThreeVector P_ttuy = G4ThreeVector(ttuy.x,ttuy.y,ttuy.z);
   G4RotationMatrix *rm_ttuy = new G4RotationMatrix();
   rm_ttuy->rotateX(ttuy.rx); rm_ttuy->rotateY(ttuy.ry); rm_ttuy->rotateZ(ttuy.rz);

   // ---- downstream: two components; above sphere, below sphere  
   // ------ below sphere 
   partParameters_t ttdby;  
   ttdby.name = "transTubeYB_dn"; ttdby.shape = "tube"; 
   ttdby.r_tor = 0.0*mm; ttdby.r_max = 4.5*mm; ttdby.r_min = ttdby.r_max - glassWall; ttdby.length = 189.0*mm;
   ttdby.x_len = 0.0*mm; ttdby.y_len = 0.0*mm; ttdby.z_len = 0.0*mm;
   ttdby.startTheta = 0.0*deg; ttdby.dTheta = 0.0*deg;
   ttdby.startPhi = 0.0*deg; ttdby.dPhi = 360.0*deg;
   ttdby.x = 0.0*mm; ttdby.y = -190.0*mm; ttdby.z = 25.4*mm;
   ttdby.rx = 90.0*deg; ttdby.ry = 0.0*deg; ttdby.rz = 0.0*deg;

   G4Tubs *transTubeDnBYShape = new G4Tubs(ttdby.name,
	                                   ttdby.r_min    ,ttdby.r_max,
	                                   ttdby.length/2.,
	                                   ttdby.startPhi ,ttdby.dPhi);

   G4ThreeVector P_ttdby = G4ThreeVector(ttdby.x,ttdby.y,ttdby.z);
   G4RotationMatrix *rm_ttdby = new G4RotationMatrix();
   rm_ttdby->rotateX(ttdby.rx); rm_ttdby->rotateY(ttdby.ry); rm_ttdby->rotateZ(ttdby.rz);

   // ------ above sphere 
   partParameters_t ttday; 
   ttday.name = "transTubeYA_dn"; ttday.shape = "tube"; 
   ttday.r_tor = 0.0*mm; ttday.r_max = 4.5*mm; ttday.r_min = ttday.r_max - glassWall; ttday.length = 20.0*mm;
   ttday.x_len = 0.0*mm; ttday.y_len = 0.0*mm; ttday.z_len = 0.0*mm;
   ttday.startTheta = 0.0*deg; ttday.dTheta = 0.0*deg;
   ttday.startPhi = 0.0*deg; ttday.dPhi = 360.0*deg;
   ttday.x = 0.0*mm; ttday.y = -61.0*mm; ttday.z = 25.4*mm;
   ttday.rx = 90.0*deg; ttday.ry = 0.0*deg; ttday.rz = 0.0*deg;

   G4Tubs *transTubeDnAYShape = new G4Tubs(ttday.name,
	                                   ttday.r_min    ,ttday.r_max,
	                                   ttday.length/2.,
	                                   ttday.startPhi ,ttday.dPhi);

   G4ThreeVector P_ttday = G4ThreeVector(ttday.x,ttday.y,ttday.z);
   G4RotationMatrix *rm_ttday = new G4RotationMatrix();
   rm_ttday->rotateX(ttday.rx); rm_ttday->rotateY(ttday.ry); rm_ttday->rotateZ(ttday.rz);

   // transfer tube post along y 
   // ---- downstream 
   partParameters_t ttpdy; 
   ttpdy.name = "transTubePost_dn"; ttpdy.shape = "tube"; 
   ttpdy.r_tor = 0.0*mm; ttpdy.r_max = 4.5*mm; ttpdy.r_min = ttpdy.r_max - glassWall; ttpdy.length = 23.0*mm;
   ttpdy.x_len = 0.0*mm; ttpdy.y_len = 0.0*mm; ttpdy.z_len = 0.0*mm;
   ttpdy.startTheta = 0.0*deg; ttpdy.dTheta = 0.0*deg;
   ttpdy.startPhi = 0.0*deg; ttpdy.dPhi = 360.0*deg;
   ttpdy.x = 0.0*mm; ttpdy.y = -22.0*mm; ttpdy.z = 259.1*mm;
   ttpdy.rx = 90.0*deg; ttpdy.ry = 0.0*deg; ttpdy.rz = 0.0*deg;

   G4Tubs *transTubePostDnShape = new G4Tubs(ttpdy.name,
	                                     ttpdy.r_min    ,ttpdy.r_max,
	                                     ttpdy.length/2.,
	                                     ttpdy.startPhi ,ttpdy.dPhi);

   G4ThreeVector P_ttpdy = G4ThreeVector(ttpdy.x,ttpdy.y,ttpdy.z);
   G4RotationMatrix *rm_ttpdy = new G4RotationMatrix();
   rm_ttpdy->rotateX(ttpdy.rx); rm_ttpdy->rotateY(ttpdy.ry); rm_ttpdy->rotateZ(ttpdy.rz);

   // ---- upstream 
   partParameters_t ttpuy; 
   ttpuy.name = "transTubePost_up"; ttpuy.shape = "tube"; 
   ttpuy.r_tor = 0.0*mm; ttpuy.r_max = 4.5*mm; ttpuy.r_min = ttpuy.r_max - glassWall; ttpuy.length = 23.0*mm;
   ttpuy.x_len = 0.0*mm; ttpuy.y_len = 0.0*mm; ttpuy.z_len = 0.0*mm;
   ttpuy.startTheta = 0.0*deg; ttpuy.dTheta = 0.0*deg;
   ttpuy.startPhi = 0.0*deg; ttpuy.dPhi = 360.0*deg;
   ttpuy.x = 0.0*mm; ttpuy.y = -22.0*mm; ttpuy.z = -259.1*mm;
   ttpuy.rx = 90.0*deg; ttpuy.ry = 0.0*deg; ttpuy.rz = 0.0*deg; 

   G4Tubs *transTubePostUpShape = new G4Tubs(ttpuy.name,
	                                     ttpuy.r_min    ,ttpuy.r_max,
	                                     ttpuy.length/2.,
	                                     ttpuy.startPhi ,ttpuy.dPhi);

   G4ThreeVector P_ttpuy = G4ThreeVector(ttpuy.x,ttpuy.y,ttpuy.z);
   G4RotationMatrix *rm_ttpuy = new G4RotationMatrix();
   rm_ttpuy->rotateX(ttpuy.rx); rm_ttpuy->rotateY(ttpuy.ry); rm_ttpuy->rotateZ(ttpuy.rz);

   // union solid 
   G4UnionSolid *glassCell;
   // target chamber + transfer tube posts
   // upstream  
   glassCell = new G4UnionSolid("gc_tc_ewud_pu" ,targetChamberShape,transTubePostUpShape,rm_ttpuy,P_ttpuy);
   // downstream 
   glassCell = new G4UnionSolid("gc_tc_ewud_pud",glassCell,transTubePostDnShape,rm_ttpdy,P_ttpdy);
   // transfer tube elbows 
   // upstream
   glassCell = new G4UnionSolid("gc_tc_ewud_pud_eu" ,glassCell,transTubeElUpShape,rm_tteu,P_tteu);
   // downstream  
   glassCell = new G4UnionSolid("gc_tc_ewud_pud_eud",glassCell,transTubeElDnShape,rm_tted,P_tted);
   // transfer tubes along z 
   // upstream
   glassCell = new G4UnionSolid("gc_tc_ewud_pud_eud_tzu" ,glassCell,transTubeUpZShape,rm_ttuz,P_ttuz);
   // downstream  
   glassCell = new G4UnionSolid("gc_tc_ewud_pud_eud_tzud",glassCell,transTubeDnZShape,rm_ttdz,P_ttdz);
   // transfer tube elbows [lower]  
   // upstream
   glassCell = new G4UnionSolid("gc_tc_ewud_pud_eud_tzud_elu" ,glassCell,transTubeElUpLoShape,rm_tteul,P_tteul);
   // downstream  
   glassCell = new G4UnionSolid("gc_tc_ewud_pud_eud_tzud_elud",glassCell,transTubeElDnLoShape,rm_ttedl,P_ttedl);
   // transfer tubes along y 
   // upstream
   glassCell = new G4UnionSolid("gc_tc_ewud_pud_eud_tzud_tyu" ,glassCell,transTubeUpYShape,rm_ttuy,P_ttuy);
   // downstream: above  
   glassCell = new G4UnionSolid("gc_tc_ewud_pud_eud_tzud_tyua",glassCell,transTubeDnAYShape,rm_ttday,P_ttday);
   // downstream: sphere   
   glassCell = new G4UnionSolid("gc_tc_ewud_pud_eud_tzud_tyuas",glassCell,transTubeSphere,rm_tts,P_tts);
   // downstream: below  
   glassCell = new G4UnionSolid("gc_tc_ewud_pud_eud_tzud_tyuasb",glassCell,transTubeDnBYShape,rm_ttdby,P_ttdby);
   // pumping chamber.  also change the name since everything is connected now 
   glassCell = new G4UnionSolid("glassCell",glassCell,pumpChamberShape,rm_pc,P_pc);

   // logical volume
   G4LogicalVolume *logicGlassCell = new G4LogicalVolume(glassCell,GetMaterial("GE180"),"logicGEnTarget_GlassCell");

   // visualization 
   G4VisAttributes *visGC = new G4VisAttributes();
   visGC->SetColour( G4Colour::White() );
   // visGC->SetForceWireframe(true);
   logicGlassCell->SetVisAttributes(visGC);

   // place the volume
   // - note that this is relative to the *target chamber* as that is the first object in the union 
   // - rotation puts the cell oriented such that the pumping chamber is vertically above
   //   and the beam enters from the side where the small sphere on the transfer tube is 
   //   closest to the upstream side 
   G4ThreeVector P_tgt_o = G4ThreeVector(0.*cm,0.*cm,0.*cm);
   G4RotationMatrix *rm_gc = new G4RotationMatrix();
   rm_gc->rotateX(0.*deg); rm_gc->rotateY(180.*deg); rm_gc->rotateZ(180.*deg);

   bool isBoolean     = true;
   bool checkOverlaps = true;

   new G4PVPlacement(rm_gc,             // rotation relative to logical mother    
                     P_tgt_o,           // position relative to logical mother      
                     logicGlassCell,   // logical volume        
                     "physGlassCell",   // name of physical volume      
                     motherLog,         // logical mother        
                     isBoolean,         // is a boolean object?     
                     0,                 // copy number     
                     checkOverlaps);    // check overlaps

   // register with DetectorConstruction object 
   fDetCon->InsertTargetVolume( logicGlassCell->GetName() ); 

}

void G4SBSTargetBuilder::BuildGEnTarget_EndWindows(G4LogicalVolume *motherLog){
   // Copper end window on 3He cell
   // - drawing number: internal from G. Cates (Assembly MK-II Drawing_july_11_2017.pdf, received June 2020)

   G4double inch = 25.4*mm; 

   // main shaft 
   partParameters_t msh; 
   msh.name = "ew_mainShaft"; msh.shape = "tube";
   msh.r_tor = 0.000*inch; msh.r_min = 0.471*inch; msh.r_max = 0.505*inch; msh.length = 0.500*inch;
   msh.x_len = 0.000*inch; msh.y_len = 0.000*inch; msh.z_len = 0.000*inch;
   msh.startTheta = 0.000*deg; msh.dTheta = 0.000*deg;
   msh.startPhi = 0.000*deg; msh.dPhi = 360.000*deg;
   msh.x = 0.000*inch; msh.y = 0.000*inch; msh.z = 0.000*inch;
   msh.rx = 0.000*deg; msh.ry = 0.000*deg; msh.rz = 0.000*deg;

   G4Tubs *mainShaft = new G4Tubs(msh.name,
	                          msh.r_min    ,msh.r_max,
	                          msh.length/2.,
	                          msh.startPhi ,msh.dPhi);

   G4ThreeVector P_msh      = G4ThreeVector(msh.x,msh.y,msh.z);
   G4RotationMatrix *rm_msh = new G4RotationMatrix();
   rm_msh->rotateX(msh.rx); rm_msh->rotateY(msh.ry); rm_msh->rotateZ(msh.rz);

   // lip 
   partParameters_t lip; 
   lip.name = "ew_lip"; lip.shape = "tube";
   lip.r_tor = 0.000*inch; lip.r_min = 0.471*inch; lip.r_max = 0.611*inch; lip.length = 0.125*inch;
   lip.x_len = 0.000*inch; lip.y_len = 0.000*inch; lip.z_len = 0.000*inch;
   lip.startTheta = 0.000*deg; lip.dTheta = 0.000*deg;
   lip.startPhi = 0.000*deg; lip.dPhi = 360.000*deg;
   lip.x = 0.000*inch; lip.y = 0.000*inch; lip.z = 0.259*inch;
   lip.rx = 0.000*deg; lip.ry = 0.000*deg; lip.rz = 0.000*deg;

   G4Tubs *lipTube = new G4Tubs(lip.name,
	                        lip.r_min    ,lip.r_max,
	                        lip.length/2.,
	                        lip.startPhi ,lip.dPhi);

   G4ThreeVector P_lip      = G4ThreeVector(lip.x,lip.y,lip.z);
   G4RotationMatrix *rm_lip = new G4RotationMatrix();
   rm_lip->rotateX(lip.rx); rm_lip->rotateY(lip.ry); rm_lip->rotateZ(lip.rz);

   // rounded lip
   partParameters_t rlip;  
   rlip.name = "ew_rlip"; rlip.shape = "sphere";
   rlip.r_tor = 0.000*inch; rlip.r_min = 0.4855*inch; rlip.r_max = 0.5455*inch; rlip.length = 0.000*inch;
   rlip.x_len = 0.000*inch; rlip.y_len = 0.000*inch; rlip.z_len = 0.000*inch;
   rlip.startTheta = 63.100*deg; rlip.dTheta = 26.900*deg;
   rlip.startPhi = 0.000*deg; rlip.dPhi = 360.000*deg;
   rlip.x = 0.000*inch; rlip.y = 0.000*inch; rlip.z = 0.300*inch;
   rlip.rx = 0.000*deg; rlip.ry = 0.000*deg; rlip.rz = 0.000*deg;

   G4Sphere *roundLip = new G4Sphere(rlip.name,
                                     rlip.r_min     ,rlip.r_max,
                                     rlip.startPhi  ,rlip.dPhi,
                                     rlip.startTheta,rlip.dTheta);

   G4ThreeVector P_rlip = G4ThreeVector(rlip.x,rlip.y,rlip.z);
   G4RotationMatrix *rm_rlip = new G4RotationMatrix();
   rm_rlip->rotateX(rlip.rx); rm_rlip->rotateY(rlip.ry); rm_rlip->rotateZ(rlip.rz);  

   // endcap 
   partParameters_t ec;
   ec.name = "ew_cap_up"; ec.shape = "sphere";
   ec.r_tor = 0.000*inch; ec.r_min = 0.4855*inch; ec.r_max = 0.4915*inch; ec.length = 0.000*inch;
   ec.x_len = 0.000*inch; ec.y_len = 0.000*inch; ec.z_len = 0.000*inch;
   ec.startTheta = 0.000*deg; ec.dTheta = 63.100*deg;
   ec.startPhi = 0.000*deg; ec.dPhi = 360.000*deg;
   ec.x = 0.000*inch; ec.y = 0.000*inch; ec.z = 0.320*inch;
   ec.rx = 0.000*deg; ec.ry = 0.000*deg; ec.rz = 0.000*deg;

   G4Sphere *endcap = new G4Sphere(ec.name,
                                   ec.r_min     ,ec.r_max,
                                   ec.startPhi  ,ec.dPhi,
                                   ec.startTheta,ec.dTheta);

   G4ThreeVector P_ec      = G4ThreeVector(ec.x,ec.y,ec.z);
   G4RotationMatrix *rm_ec = new G4RotationMatrix();
   rm_ec->rotateX(ec.rx); rm_ec->rotateY(ec.ry); rm_ec->rotateZ(ec.rz);   

   // labels 
   G4String label1 = "ew_ms_l"   ;
   G4String label2 = "ew_ms_l_rl";
   G4String label3 = "endWindow" ;

   // union solid 
   G4UnionSolid *endWindow;
   // main shaft + lip 
   endWindow = new G4UnionSolid(label1,mainShaft,lipTube ,rm_lip ,P_lip);
   // add rounded lip 
   endWindow = new G4UnionSolid(label2,endWindow,roundLip,rm_rlip,P_rlip);
   // endcap  
   endWindow = new G4UnionSolid(label3,endWindow,endcap  ,rm_ec  ,P_ec); 

   // visualization
   G4VisAttributes *vis = new G4VisAttributes();
   vis->SetColour( G4Colour::Red() );
   vis->SetForceWireframe(true);

   // define coordinates and rotations for placement  
   // index 0 = upstream, 1 = downstream 

   G4double x_ew[2] = {0,0}; 
   G4double y_ew[2] = {0,0}; 
   G4double z_ew[2] = {0,0}; 

   G4double z0 = 571.7*mm/2. + 0.5*msh.length;
   z_ew[0] = (-1.)*z0; 
   z_ew[1] = z0; 

   G4double rx[2] = {0.*deg  ,0.*deg};  
   G4double ry[2] = {180.*deg,0.*deg};  
   G4double rz[2] = {0.*deg  ,0.*deg}; 

   bool isBoolean     = true; 
   bool checkOverlaps = true; 
 
   // logical volumes (for an array of logical volume pointers)  
   G4LogicalVolume **logicEndWindow = new G4LogicalVolume*[2];

   char logicName[200],physName[200]; 

   for(int i=0;i<2;i++){
      // create logical volume
      sprintf(logicName,"logicGEnTarget_EndWindow_%d",i);  
      logicEndWindow[i] = new G4LogicalVolume(endWindow,GetMaterial("Copper"),logicName);
      logicEndWindow[i]->SetVisAttributes(vis);  
      // position and rotation 
      G4ThreeVector P_ew      = G4ThreeVector(x_ew[i],y_ew[i],z_ew[i]);
      G4RotationMatrix *rm_ew = new G4RotationMatrix();
      rm_ew->rotateX(rx[i]); rm_ew->rotateY(ry[i]); rm_ew->rotateZ(rz[i]);
      // physical name 
      sprintf(physName,"physGEnTarget_EndWindow_%d",i);  
      // place the volume  
      new G4PVPlacement(rm_ew,               // rotation relative to logic mother
	                P_ew,                // position relative to logic mother 
	                logicEndWindow[i],   // logical volume 
	                physName,            // name 
	                motherLog,           // logical mother volume is the target chamber 
	                isBoolean,           // no boolean operations 
	                i,                   // copy number 
	                checkOverlaps);      // check overlaps
      // register with DetectorConstruction object
      fDetCon->InsertTargetVolume( logicEndWindow[i]->GetName() );  
   }
 
}

void G4SBSTargetBuilder::BuildGEnTarget_PolarizedHe3(G4LogicalVolume *motherLog){
   // Polarized 3He
   // - takes the form of the target chamber of the glass cell 
   
   G4double inch       = 25.4*mm; 
   G4double glassWall  = 1.0*mm;   // estimate 
   G4double tubeLength = 571.7*mm; // 579.0*mm;  

   // target chamber component 
   partParameters_t tc; 
   tc.name   = "targetChamber"; tc.shape  = "tube";
   tc.r_tor  = 0.*mm; tc.r_min = 0.*mm; tc.r_max = 10.5*mm - glassWall; tc.length = tubeLength; 
   tc.startTheta = 0*deg; tc.dTheta = 0*deg;
   tc.startPhi = 0*deg; tc.dPhi = 360*deg;
   tc.x = 0*mm; tc.y = 0*mm; tc.z = 0*mm;
   tc.rx = 0*deg; tc.ry = 0*deg; tc.rz = 0*deg;

   G4Tubs *tcShape = new G4Tubs("He3_tc",
	                        tc.r_min    ,tc.r_max,
	                        tc.length/2.,
	                        tc.startPhi ,tc.dPhi);

   // end window, upstream
   // NOTE: make this radius 0.5 mm smaller than the Cu window radius for the equivalent geometry components  
   // ---- main shaft 
   partParameters_t mshu; 
   mshu.name = "ew_mainShaft_up"; mshu.shape = "tube";
   // mshu.r_tor = 0.0*mm; mshu.r_min = 0.0*mm; mshu.r_max = 10.5*mm - glassWall; mshu.length = 0.500*inch;
   mshu.r_tor = 0.0*mm; mshu.r_min = 0.0*mm; mshu.r_max = 0.451*inch; mshu.length = 0.500*inch;
   mshu.startTheta = 0.0*deg; mshu.dTheta = 0.0*deg;
   mshu.startPhi = 0.0*deg; mshu.dPhi = 360.0*deg;
   mshu.x = 0.0*mm; mshu.y = 0.0*mm; mshu.z = -11.504*inch;
   mshu.rx = 0.0*deg; mshu.ry = 0.0*deg; mshu.rz = 0.0*deg;

   G4Tubs *mainShaft_up = new G4Tubs(mshu.name,
	                             mshu.r_min    ,mshu.r_max,
	                             mshu.length/2.,
	                             mshu.startPhi ,mshu.dPhi);

   G4ThreeVector P_mshu      = G4ThreeVector(mshu.x,mshu.y,mshu.z);
   G4RotationMatrix *rm_mshu = new G4RotationMatrix();
   rm_mshu->rotateX(mshu.rx); rm_mshu->rotateY(mshu.ry); rm_mshu->rotateZ(mshu.rz);

  // ---- lip 
  // NOTE: make this radius 0.5 mm smaller than the Cu window radius for the equivalent geometry components  
  partParameters_t lipu;
  lipu.name = "ew_lip_up"; lipu.shape = "tube";
  // lipu.r_tor = 0.0*mm; lipu.r_min = 0.0*mm; lipu.r_max = 10.5*mm - glassWall; lipu.length = 0.125*inch;
  lipu.r_tor = 0.0*mm; lipu.r_min = 0.0*mm; lipu.r_max = 0.451*inch; lipu.length = 0.125*inch;
  lipu.startTheta = 0.0*deg; lipu.dTheta = 0.0*deg;
  lipu.startPhi = 0.0*deg; lipu.dPhi = 360.0*deg;
  lipu.x = 0.0*mm; lipu.y = 0.0*mm; lipu.z = -11.763*inch;
  lipu.rx = 0.0*deg; lipu.ry = 0.0*deg; lipu.rz = 0.0*deg; 

  G4Tubs *lip_up = new G4Tubs(lipu.name,
	                      lipu.r_min    ,lipu.r_max,
	                      lipu.length/2.,
	                      lipu.startPhi ,lipu.dPhi);

  G4ThreeVector P_lipu      = G4ThreeVector(lipu.x,lipu.y,lipu.z);
  G4RotationMatrix *rm_lipu = new G4RotationMatrix();
  rm_lipu->rotateX(lipu.rx); rm_lipu->rotateY(lipu.ry); rm_lipu->rotateZ(lipu.rz); 

  // // ---- rounded lip 
  // partParameters_t rlipu; 
  // rlipu.name = "ew_rlip_up"; rlipu.shape = "sphere";
  // rlipu.r_tor = 0.0*mm; rlipu.r_min = 0.0*mm; rlipu.r_max = 0.4855*inch; rlipu.length = 0.0*mm;
  // rlipu.startTheta = 63.1*deg; rlipu.dTheta = 26.9*deg; 
  // rlipu.startPhi = 0.0*deg; rlipu.dPhi = 360.0*deg;
  // rlipu.x = 0.0*mm; rlipu.y = 0.0*mm; rlipu.z = -11.804*inch; 
  // rlipu.rx = 0.0*deg; rlipu.ry = 180.0*deg; rlipu.rz = 0.0*deg;

  // G4Sphere *roundLip_up = new G4Sphere(rlipu.name,
  //                                      rlipu.r_min     ,rlipu.r_max,
  //                                      rlipu.startPhi  ,rlipu.dPhi,
  //                                      rlipu.startTheta,rlipu.dTheta);

  // G4ThreeVector P_rlipu      = G4ThreeVector(rlipu.x,rlipu.y,rlipu.z);
  // G4RotationMatrix *rm_rlipu = new G4RotationMatrix();
  // rm_rlipu->rotateX(rlipu.rx); rm_rlipu->rotateY(rlipu.ry); rm_rlipu->rotateZ(rlipu.rz);

  // // ---- endcap
  // partParameters_t ecu; 
  // ecu.name = "ew_cap_up"; ecu.shape = "sphere";
  // ecu.r_tor = 0.0*mm; ecu.r_min = 0.0*mm; ecu.r_max = 0.4855*inch; ecu.length = 0.0*mm;
  // ecu.startTheta = 0.0*deg; ecu.dTheta = 63.1*deg;
  // ecu.startPhi = 0.0*deg; ecu.dPhi = 360.0*deg;
  // ecu.x = 0.0*mm; ecu.y = 0.0*mm; ecu.z = -11.824*mm;
  // ecu.rx = 0.0*deg; ecu.ry = 180.0*deg; ecu.rz = 0.0*deg;

  // G4Sphere *endcap_up = new G4Sphere(ecu.name,
  //                                    ecu.r_min     ,ecu.r_max,
  //                                    ecu.startPhi  ,ecu.dPhi,
  //                                    ecu.startTheta,ecu.dTheta);

  // G4ThreeVector P_ecu      = G4ThreeVector(ecu.x,ecu.y,ecu.z);
  // G4RotationMatrix *rm_ecu = new G4RotationMatrix();
  // rm_ecu->rotateX(ecu.rx); rm_ecu->rotateY(ecu.ry); rm_ecu->rotateZ(ecu.rz);

  // simplified geometry.  replaces rlip and endcap  
  // need a fudge factor of delta = 0.5*mm to avoid overlap with Cu endcap 
  G4double delta = 0.5*mm; 
  partParameters_t segu;
  segu.name = "seg_up"; segu.shape = "sphere";  
  segu.r_tor = 0.000*inch; segu.r_min = 0.000*inch; segu.r_max = 0.4855*inch - delta; segu.length = 0.000*inch;
  segu.x_len = 0.000*inch; segu.y_len = 0.000*inch; segu.z_len = 0.000*inch;
  segu.startTheta = 0.000*deg; segu.dTheta = 90.000*deg;
  segu.startPhi = 0.000*deg; segu.dPhi = 360.000*deg;
  segu.x = 0.000*inch; segu.y = 0.000*inch; segu.z = -11.804*inch;
  segu.rx = 0.000*deg; segu.ry = 180.000*deg; segu.rz = 0.000*deg;

  G4Sphere *seg_up = new G4Sphere(segu.name,
                                  segu.r_min     ,segu.r_max,
                                  segu.startPhi  ,segu.dPhi,
                                  segu.startTheta,segu.dTheta);

  G4ThreeVector P_segu = G4ThreeVector(segu.x,segu.y,segu.z);
  G4RotationMatrix *rm_segu = new G4RotationMatrix();
  rm_segu->rotateX(segu.rx); rm_segu->rotateY(segu.ry); rm_segu->rotateZ(segu.rz);  

  // end window, downstream
  // - can exploit symmetry here  
  // ---- main shaft 
  partParameters_t mshd = mshu; 
  mshd.name = "mshd_dn";
  mshd.z *= -1.;

  G4Tubs *mainShaft_dn = new G4Tubs(mshd.name,
	                            mshd.r_min    ,mshd.r_max,
	                            mshd.length/2.,
	                            mshd.startPhi ,mshd.dPhi);

  G4ThreeVector P_mshd      = G4ThreeVector(mshd.x,mshd.y,mshd.z);
  G4RotationMatrix *rm_mshd = new G4RotationMatrix();
  rm_mshd->rotateX(mshd.rx); rm_mshd->rotateY(mshd.ry); rm_mshd->rotateZ(mshd.rz);
 
  // ---- lip 
  partParameters_t lipd = lipu; 
  lipd.name = "lipd_dn";
  lipd.z *= -1; 

  G4Tubs *lip_dn = new G4Tubs(lipd.name,
                              lipd.r_min    ,lipd.r_max,
                              lipd.length/2.,
                              lipd.startPhi ,lipd.dPhi);

  G4ThreeVector P_lipd      = G4ThreeVector(lipd.x,lipd.y,lipd.z);
  G4RotationMatrix *rm_lipd = new G4RotationMatrix();
  rm_lipd->rotateX(lipd.rx); rm_lipd->rotateY(lipd.ry); rm_lipd->rotateZ(lipd.rz);

  // // ---- rounded lip 
  // partParameters_t rlipd = rlipu;
  // rlipd.name = "rlipd_dn";
  // rlipd.z *= -1.;
  // rlipd.ry = 0.*deg;

  // G4Sphere *roundLip_dn = new G4Sphere(rlipd.name,
  //                                      rlipd.r_min     ,rlipd.r_max,
  //                                      rlipd.startPhi  ,rlipd.dPhi,
  //                                      rlipd.startTheta,rlipd.dTheta);

  // G4ThreeVector P_rlipd      = G4ThreeVector(rlipd.x,rlipd.y,rlipd.z);
  // G4RotationMatrix *rm_rlipd = new G4RotationMatrix();
  // rm_rlipd->rotateX(rlipd.rx); rm_rlipd->rotateY(rlipd.ry); rm_rlipd->rotateZ(rlipd.rz);
 
  // // ---- endcap 
  // partParameters_t ecd = ecu;
  // ecd.name = "ecd_dn";
  // ecd.z *= -1.;
  // ecd.ry = 0.*deg;

  // G4Sphere *endcap_dn = new G4Sphere(ecd.name,
  //                                    ecd.r_min     ,ecd.r_max,
  //                                    ecd.startPhi  ,ecd.dPhi,
  //                                    ecd.startTheta,ecd.dTheta);

  // G4ThreeVector P_ecd      = G4ThreeVector(ecd.x,ecd.y,ecd.z);
  // G4RotationMatrix *rm_ecd = new G4RotationMatrix();
  // rm_ecd->rotateX(ecd.rx); rm_ecd->rotateY(ecd.ry); rm_ecd->rotateZ(ecd.rz); 

  // ---- simplified hemisphere. replaces rlip and endcap 
  partParameters_t segd = segu;
  segd.name = "seg_dn"; 
  segd.z *= -1.;
  segd.ry = 0.*deg;  

  G4Sphere *seg_dn = new G4Sphere(segd.name,
                                  segd.r_min     ,segd.r_max,
                                  segd.startPhi  ,segd.dPhi,
                                  segd.startTheta,segd.dTheta);

  G4ThreeVector P_segd = G4ThreeVector(segd.x,segd.y,segd.z);
  G4RotationMatrix *rm_segd = new G4RotationMatrix();
  rm_segd->rotateX(segd.rx); rm_segd->rotateY(segd.ry); rm_segd->rotateZ(segd.rz);  

  // create the union solid 
  G4UnionSolid *he3Tube;
  // main shaft + upstream "window"  
  he3Tube = new G4UnionSolid("tc_um"       ,tcShape,mainShaft_up,rm_mshu ,P_mshu );
  he3Tube = new G4UnionSolid("tc_uml"      ,he3Tube,lip_up      ,rm_lipu ,P_lipu );
  he3Tube = new G4UnionSolid("tc_umls"     ,he3Tube,seg_up      ,rm_segu ,P_segu ); 
  // he3Tube = new G4UnionSolid("tc_umlr"      ,he3Tube,roundLip_up ,rm_rlipu,P_rlipu);
  // he3Tube = new G4UnionSolid("tc_umlre"     ,he3Tube,endcap_up   ,rm_ecu  ,P_ecu  );
  // downstream end window  
  he3Tube = new G4UnionSolid("tc_umls_dm"  ,he3Tube,mainShaft_dn,rm_mshd ,P_mshd );
  he3Tube = new G4UnionSolid("tc_umls_dml" ,he3Tube,lip_dn      ,rm_lipd ,P_lipd );
  he3Tube = new G4UnionSolid("he3Tube"     ,he3Tube,seg_dn      ,rm_segd ,P_segd ); 
  // he3Tube = new G4UnionSolid("tc_umlre_dmlr",he3Tube,roundLip_dn ,rm_rlipd,P_rlipd);
  // he3Tube = new G4UnionSolid("he3Tube"      ,he3Tube,endcap_dn   ,rm_ecd  ,P_ecd  );

  // set the color of He3 
  G4VisAttributes *visHe3 = new G4VisAttributes();
  visHe3->SetColour( G4Colour::Yellow() );
  // visHe3->SetForceWireframe(true);  

  // logical volume of He3
  G4LogicalVolume *logicHe3 = new G4LogicalVolume(he3Tube,GetMaterial("pol3He"),"logicGEnTarget_polHe3");
  logicHe3->SetVisAttributes(visHe3);

  // placement of He3 is *inside target chamber*  
  G4ThreeVector posHe3 = G4ThreeVector(0.*cm,0.*cm,0.*cm);

  bool isBoolean = true;
  bool checkOverlaps = true;

  new G4PVPlacement(0,                      // rotation
                    posHe3,                 // position 
                    logicHe3,               // logical volume 
                    "physGEnTarget_polHe3", // name 
                    motherLog,              // logical mother volume  
                    isBoolean,              // boolean operations?  
                    0,                      // copy number 
                    checkOverlaps);         // check overlaps 

  // register with DetectorConstruction object
  fDetCon->InsertTargetVolume( logicHe3->GetName() );  

}

void G4SBSTargetBuilder::BuildGEnTarget_HelmholtzCoils(const int config,const std::string type,G4LogicalVolume *motherLog){
   // Helmholtz coils for B fields
   // - no magnetic fields are implemented!
   // - materials: outer shell of G10.  thickness based on type
   //              core is solid aluminum
   // - config: kSBS_GEN_677, kSBS_GEN_1018, kSBS_GEN_368, kSBS_GEN_146
   //           different rotation angle based on index number (Q2 setting)   
   // - types: maj = large radius coil pair
   //          min = small radius coil pair 
   //          rfy = RF coil pair, aligned along the vertical (y) axis  
   // - distance between coils D = 0.5(rmin+rmax), roughly the major radius of the tube   
   //   - coil n (placed at -D/2), upstream  
   //   - coil p (placed at +D/2), downstream 
   // - Drawing number: A09016-03-08-0000

   // coil name
   char coilName_n[200],coilName_p[200];    
   // shell names  
   char shellName_n[200],shellName_p[200];

   sprintf(coilName_n ,"HH_%s_n",type.c_str());
   sprintf(coilName_p ,"HH_%s_p",type.c_str());

   sprintf(shellName_n ,"shellHH_%s_n",type.c_str());
   sprintf(shellName_p ,"shellHH_%s_p",type.c_str());

   // parameters 
   partParameters_t cn; 
   cn.shape = "tube";
   cn.startTheta = 0.0*deg; cn.dTheta = 0.0*deg;
   cn.startPhi   = 0.0*deg; cn.dPhi   = 360.0*deg;
     
   cn.name = coilName_n;

   if(type.compare("maj")==0){
     cn.r_min = 722.3*mm; cn.r_max = 793.8*mm; cn.length = 80.9*mm; 
     cn.x     = 0.0*mm;   cn.y     = 150.1*mm; cn.z      = 0.0*mm;
     cn.rx    = 0.0*deg;  cn.ry    = 90.0*deg; cn.rz     = 0.0*deg;
   }else if(type.compare("rfy")==0){
     cn.r_min = 488.9*mm; cn.r_max = 520.7*mm; cn.length = 14.1*mm; 
     cn.x     = 0.0*mm;   cn.y     = 0.0*mm;   cn.z      = 0.0*mm;
     cn.rx    = 90.0*deg; cn.ry    = 0.0*deg;  cn.rz     = 0.0*deg;
   }else if(type.compare("min")==0){
     cn.r_min = 631.8*mm; cn.r_max = 677.9*mm; cn.length = 64.8*mm; 
     cn.x     = 0.0*mm;   cn.y     = 150.1*mm; cn.z      = 0.0*mm;
     cn.rx    = 0.0*deg;  cn.ry    = 0.0*deg;  cn.rz     = 0.0*deg;
   }else{
      std::cout << "[G4SBSTargetBuilder::BuildGEnTarget_HelmholtzCoils]: Invalid type = " << type << std::endl;
      exit(1);
   }
 
   // coil parameters   
   G4double D      = 0.5*(cn.r_min + cn.r_max);          // helmholtz separation D = R = 0.5(rmin + rmax) 
   G4double shWall = 0;

   G4double inch   = 2.54*cm; 

   if( type.compare("maj")==0 ) shWall = 5.0*mm;         // FIXME: Estimates for now! 
   if( type.compare("min")==0 ) shWall = 5.0*mm;         // FIXME: Estimates for now! 
   if( type.compare("rfy")==0 ) shWall = 0.030*inch;

   // // upstream coil 
   // partParameters_t cp = cn; 
   // cp.name = coilName_p;
  
   // shell 
   // ---- upstream  
   partParameters_t cns;
   cns.name     = shellName_n;
   cns.r_min    = cn.r_min - shWall;
   cns.r_max    = cn.r_max + shWall;
   cns.length   = cn.length + 2.*shWall; // this is so we have the wall on both sides
   cns.startPhi = 0.*deg;
   cns.dPhi     = 360.*deg;

   G4Tubs *cnsTube = new G4Tubs(cns.name,
	                        cns.r_min    ,cns.r_max,
	                        cns.length/2.,
	                        cns.startPhi ,cns.dPhi);


   // need to create the core we subtract from the solid shell 
   partParameters_t cns_core = cn;
   cns_core.name = cn.name + "_core";
   G4Tubs *cnsTube_core = new G4Tubs(cns_core.name,
	                             cns_core.r_min    ,cns_core.r_max,
	                             cns_core.length/2.,
	                             cns_core.startPhi ,cns_core.dPhi);

   // subtract the core 
   G4SubtractionSolid *coilShell = new G4SubtractionSolid("cns_sub",cnsTube,cnsTube_core,0,G4ThreeVector(0,0,0));

// this seems repetitive (unnecessary?) 
//    // ---- downstream  
//    partParameters_t cps;
//    cps.name     = shellName_p;
//    cps.r_min    = cp.r_min - shWall;
//    cps.r_max    = cp.r_max + shWall;
//    cps.length   = cp.length + 2.*shWall; // this is so we have the wall on both sides
//    cps.startPhi = 0.*deg;
//    cps.dPhi     = 360.*deg;
// 
//    G4Tubs *cpsTube = new G4Tubs(cps.name,
// 	                        cps.r_min    ,cps.r_max,
// 	                        cps.length/2.,
// 	                        cps.startPhi ,cps.dPhi);
//
//    partParameters_t cps_core = cp;
//    cps_core.name = cp.name + "_core";
//    G4Tubs *cpsTube_core = new G4Tubs(cps_core.name,
// 	                             cps_core.r_min    ,cps_core.r_max,
// 	                             cps_core.length/2.,
// 	                             cps_core.startPhi ,cps_core.dPhi);
//    // subtract the core 
//    G4SubtractionSolid *coilShell_p = new G4SubtractionSolid("cps_sub",cpsTube,cpsTube_core,0,G4ThreeVector(0,0,0));


   // place volumes 
   // determine coordinates for placement based on type 

   // additional rotation to match engineering drawings (number A09016-03-08-0000) 
   G4double dry=0;
   if( type.compare("maj")==0 || type.compare("min")==0 ){
      if(config==G4SBS::kSBS_GEN_146 || config==G4SBS::kSBS_GEN_368)  dry = 43.5*deg;
      if(config==G4SBS::kSBS_GEN_677 || config==G4SBS::kSBS_GEN_1018) dry = 10.0*deg;
   }

   // adjust for y rotation.  
   // WARNING: should this exactly follow rotated coordinates?
   // rotation about y 
   // x' =  xcos + zsin 
   // z' = -xsin + zcos

   // rotation 
   G4double RX = cn.rx;
   G4double RY = cn.ry + dry;
   G4double RZ = cn.rz;

   G4RotationMatrix *rms = new G4RotationMatrix();
   rms->rotateX(RX); rms->rotateY(RY); rms->rotateZ(RZ);

   // sine and cosine of total rotation angle about y axis
   G4double COS_TOT = cos(RY);
   G4double SIN_TOT = sin(RY);

   bool isBoolean     = true;
   bool checkOverlaps = true;

   G4double x0 = cn.x;
   G4double y0 = cn.y;
   G4double z0 = cn.z;

   G4double x[2] = {x0,x0}; 
   G4double y[2] = {y0,y0}; 
   G4double z[2] = {z0,z0}; 

   if(type.compare("maj")==0){
      x[0] = x0 - (D/2.)*fabs(SIN_TOT); 
      x[1] = x0 + (D/2.)*fabs(SIN_TOT); 
      z[0] = z0 - (D/2.)*fabs(COS_TOT); 
      z[1] = z0 + (D/2.)*fabs(COS_TOT); 
   }else if(type.compare("min")==0){
      x[0] = x0 + (D/2.)*fabs(SIN_TOT); 
      x[1] = x0 - (D/2.)*fabs(SIN_TOT); 
      // note sign doesn't flip for z! 
      z[0] = z0 - (D/2.)*fabs(COS_TOT); 
      z[1] = z0 + (D/2.)*fabs(COS_TOT); 
   }else if(type.compare("rfy")==0){
      y[0] = y0 - D/2.; // below
      y[1] = y0 + D/2.; // above 
   }

   // create logical volumes 
   G4VisAttributes *visCS = new G4VisAttributes();
   visCS->SetForceWireframe();
   if(type.compare("maj")==0) visCS->SetColour( G4Colour::Red()   );
   if(type.compare("rfy")==0) visCS->SetColour( G4Colour::Green() );
   if(type.compare("min")==0) visCS->SetColour( G4Colour::Blue()  );

   // logical volumes
   G4LogicalVolume **logicHelmholtzShell = new G4LogicalVolume*[2]; 

   char logicShellName[200],physShellName[200]; 
 
   for(int i=0;i<2;i++){
      sprintf(logicShellName,"logicGEnTarget_HHCoilShell_%s_%d",type.c_str(),i); 
      logicHelmholtzShell[i] = new G4LogicalVolume(coilShell,GetMaterial("NEMAG10"),logicShellName);
      logicHelmholtzShell[i]->SetVisAttributes(visCS); 
      // place the coil shell 
      G4ThreeVector P_cs = G4ThreeVector(x[i],y[i],z[i]);
      sprintf(physShellName,"physGEnTarget_HHCoilShell_%s_%d",type.c_str(),i); 
      new G4PVPlacement(rms,                      // rotation relative to logic mother     
	                P_cs,                     // position relative to logic mother  
	                logicHelmholtzShell[i],   // logical volume          
	                physShellName,            // physical name     
	                motherLog,                // logical mother volume 
	                isBoolean,                // boolean solid?   
	                i,                        // copy number  
	                checkOverlaps);           // check overlaps 
      // register with DetectorConstruction object
      fDetCon->InsertTargetVolume( logicHelmholtzShell[i]->GetName() ); 
   }

   // aluminum core -- goes *inside* the shell  
   G4VisAttributes *visCoil = new G4VisAttributes();
   visCoil->SetColour( G4Colour::Grey() );

   // cylindrical geometry 
   G4Tubs *cnTube = new G4Tubs(cn.name,
                               cn.r_min,cn.r_max,
                               cn.length/2.,
                               cn.startPhi,cn.dPhi);

//   G4Tubs *cpTube = new G4Tubs(cp.name,
//                               cp.r_min,cp.r_max,
//                               cp.length/2.,
//                               cp.startPhi,cp.dPhi);

   // logical volume of the coil cores
   char logicCoilName[200],physCoilName[200];
   G4LogicalVolume **logicHelmholtz = new G4LogicalVolume*[2];

   // placement.  note logic mother is the shell!  no rotation or position offsets needed  
   // std::string physCoilName = "physCoil_" + type;  
   for(int i=0;i<2;i++){
      sprintf(logicCoilName,"logicGEnTarget_HHCoil_%s_%d",type.c_str(),i); 
      logicHelmholtz[i] = new G4LogicalVolume(cnTube,GetMaterial("Aluminum"),logicCoilName);
      logicHelmholtz[i]->SetVisAttributes(visCoil);
      sprintf(physShellName,"physGEnTarget_HHCoil_%s_%d",type.c_str(),i); 
      new G4PVPlacement(0,                        // rotation relative to logic mother             
	                G4ThreeVector(0,0,0),     // position relative to logic mother             
	                logicHelmholtz[i],        // logical volume                                
	                physCoilName,             // physical name                                
	                logicHelmholtzShell[i],   // logical mother volume                          
	                isBoolean,                // boolean solid?                             
	                i,                        // copy number                              
	                checkOverlaps);           // check overlaps                          
      // register with DetectorConstruction object
      fDetCon->InsertTargetVolume( logicHelmholtz[i]->GetName() ); 
   }

}

void G4SBSTargetBuilder::BuildGEnTarget_Shield(const int config,G4LogicalVolume *motherLog){
   // Shield box for the target magnetic field 
   // - Material: Carbon steel 1008
   // - config: kSBS_GEN_146, kSBS_GEN_368, kSBS_GEN_677, kSBS_GEN_1018
   //           Different cutaways based on index number (Q2 setting)   
   // - The shield is actually two layers
   //   - each layer is 0.25" thick
   //   - outer surfaces of layers separated by 1.29" 
   //     => 1.29 - 0.25 = 1.04" center-to-center distance 
   // - Drawing number: A09016-03-05-0000, A09016-03-05-0800

   // constants from drawings  
   G4double inch = 2.54*cm;
   G4double wall = 0.25*inch;
   G4double sp   = 0.79*inch;  // inner-spacing: |<-s->|

   // general part details 
   partParameters_t sh; 
   sh.name = "shield"; sh.shape = "box"; 
   sh.r_tor = 0.0*mm; sh.r_min = 0.0*mm; sh.r_max = 0.0*mm; sh.length = 0.0*mm;
   sh.x_len = 2252.9*mm; sh.y_len = 2560.3*mm; sh.z_len = 2252.9*mm;
   sh.startTheta = 0.0*deg; sh.dTheta = 0.0*deg;
   sh.startPhi = 0.0*deg; sh.dPhi = 0.0*deg;
   sh.x = 0.0*mm; sh.y = 0.0*mm; sh.z = 0.0*mm;
   sh.rx = 0.0*deg; sh.ry = 0.0*deg; sh.rz = 0.0*deg;

   //---- window cuts
   // FIXME: these will change!     
   // downstream, along beam.  sizes are estimates! 
   G4double xw = 9.48*inch;
   G4double yw = 9.48*inch;
   G4double zw = 9.48*inch;
   G4Box *windowCut_dn = new G4Box("windowCut_dn",xw/2.,yw/2.,zw/2.);
   G4ThreeVector Pw_dn = G4ThreeVector(sh.x_len/2.,0.,sh.z_len/3.);  // position of cut 

   G4double door = 40.5*inch; // for OWU2A door panel (from JT model)  

   // downstream, beam left 
   // x = 4.62 + ??, y = 28.44", z = 4.62 + ?? 
   // drawings: A09016-03-05-0851  

   G4double xw_bl=0,yw_bl=0,zw_bl=0,ys=0;

   if(config==G4SBS::kSBS_GEN_new){
      // FIXME: This is an estimate 
      // new cut as of 6/27/20 (new design from Bert)
      xw_bl = 10.*cm; // arbitrary size
      yw_bl = 0.8*sh.y_len;
      zw_bl = door;
      // coordinates 
      ys    = (-1.)*(sh.y_len/2.-yw_bl/2.);          // this should center the cut properly  
   }else{
      // original cut as per drawing A09016-03-0851  
      xw_bl = 15.*inch; 
      yw_bl = 28.44*inch;
      zw_bl = 15.*inch;
      // coordinates 
      ys    = 0.*cm;
   }

   // coordinates 
   G4double xs = sh.x_len/2.;                         // distance to midpoint of door, aligns door close to edge 
   G4double zs = -zw_bl/2.;                           // right on top of the right side wall (before rotation)  
   G4ThreeVector Pw_bl_dn = G4ThreeVector(xs,ys,zs);  // position of cut 

   G4Box *windowCut_beamLeft_dn = new G4Box("windowCut_beamLeft_dn",xw_bl/2.,yw_bl/2.,zw_bl/2.);

   // downstream, beam right
   // for now consider a single large cut 
   // large opening; distance along x and z = panel 1 + door w/handles + panel 3
   // panel 1, drawing A09016-03-05-0811: x = 8.62" , y = 21.41", z = 8.62" 
   // panel 2, drawing                  : x = 6.62" , y = 21.41", z = 6.62" 
   // door   , drawing A09016-03-05-0841: x = 16.70", y = 21.41", z = 16.70" 
   // panel 3, drawing A09016-03-05-0831: x = 5.12" , y = 21.41", z = 5.12" 
   G4double p1 = 8.62*inch;
   G4double p2 = 6.62*inch;
   G4double p3 = 5.12*inch;
   G4double dh = 16.70*inch;

   G4double dx=0,dx0=5.*cm;
   G4double xw_br=0,yw_br=0;
   G4double zw_br=10.*cm; // arbitrary cut depth in z; just need enough to break through 
   if(config==G4SBS::kSBS_GEN_146){
      dx    = dx0 + p1 + p2 + p3;
      xw_br = dh;
      yw_br = 21.41*inch;
      // coordinates 
      ys    = 0.*cm;
   }else if(config==G4SBS::kSBS_GEN_368){
      dx    = dx0 + p2 + p3;
      xw_br = dh;
      yw_br = 21.41*inch;
      // coordinates 
      ys    = 0.*cm;
   }else if(config==G4SBS::kSBS_GEN_677){
      dx    = dx0 + p3;
      xw_br = dh;
      yw_br = 21.41*inch;
      // coordinates 
      ys    = 0.*cm;
   }else if(config==G4SBS::kSBS_GEN_1018){
      dx    = dx0;
      xw_br = dh;
      yw_br = 21.41*inch;
      // coordinates 
      ys    = 0.*cm;
   }else if(config==G4SBS::kSBS_GEN_full){
      // full window cut 
      dx    = dx0;
      xw_br = p1 + p2 + p3 + dh;
      yw_br = 21.41*inch;
      // coordinates 
      ys    = 0.*cm;
   }else if(config==G4SBS::kSBS_GEN_new){
      // 6/27/20: new design from Bert 
      dx    = dx0;
      xw_br = door;
      yw_br = 0.8*sh.y_len;
      // coordinates
      ys = (-1.)*(sh.y_len/2.-yw_br/2.);   // this should center the cut properly  
   }else{
      std::cout << "[G4SBSTargetBuilder::BuildGEnTarget_Shield]: Invalid configuration = " << config << std::endl;
      exit(1);
   }

   xs = sh.x_len/2. - xw_br/2. - dx;    // distance to midpoint of door, aligns door close to edge 
   zs = sh.z_len/2.;                    // right on top of the right side wall (before rotation) 

   G4Box *windowCut_beamRight_dn = new G4Box("windowCut_beamRight_dn",xw_br/2.,yw_br/2.,zw_br/2.);
   G4ThreeVector Pw_br_dn        = G4ThreeVector(xs,ys,zs);  // position of cut 

   // upstream, along beam  
   G4Box *windowCut_up = new G4Box("windowCut_up",xw/2.,yw/2.,zw/2.);
   G4ThreeVector Pw_up = G4ThreeVector(-sh.x_len/2.,0.,-sh.z_len/3.);  // position of cut 

   //---- shield: put everything together   

   // outer box 
   G4Box *outer = new G4Box("outer"   ,sh.x_len/2.,sh.y_len/2.,sh.z_len/2.);
   // cut away inner material  
   G4double xc = sh.x_len - wall*2.;
   G4double yc = sh.y_len - wall*2.;
   G4double zc = sh.z_len - wall*2.;
   G4Box *outerCut = new G4Box("outerCut",xc/2.,yc/2.,zc/2.);

   // subtract the parts 
   G4SubtractionSolid *outerShield = new G4SubtractionSolid("outerShield_1",outer,outerCut,0,G4ThreeVector(0,0,0));
   outerShield = new G4SubtractionSolid("outerShield_2",outerShield,windowCut_dn,          0,Pw_dn   );
   outerShield = new G4SubtractionSolid("outerShield_3",outerShield,windowCut_beamLeft_dn ,0,Pw_bl_dn);
   outerShield = new G4SubtractionSolid("outerShield_4",outerShield,windowCut_beamRight_dn,0,Pw_br_dn);
   outerShield = new G4SubtractionSolid("outerShield"  ,outerShield,windowCut_up          ,0,Pw_up   );

   // inner box 
   partParameters_t sh_inner = sh;
   sh_inner.x_len = sh.x_len - 2.*wall - 2.*sp;
   sh_inner.y_len = sh.y_len - 2.*wall - 2.*sp;
   sh_inner.z_len = sh.z_len - 2.*wall - 2.*sp;

   G4Box *inner = new G4Box("inner",sh_inner.x_len/2.,sh_inner.y_len/2.,sh_inner.z_len/2.);
   // cut away inner material  
   xc = sh_inner.x_len - wall*2.;
   yc = sh_inner.y_len - wall*2.;
   zc = sh_inner.z_len - wall*2.;
   G4Box *innerCut = new G4Box("innerCut",xc/2.,yc/2.,zc/2.);

   // subtract the parts 
   G4SubtractionSolid *innerShield = new G4SubtractionSolid("innerShield_1",inner,innerCut,0,G4ThreeVector(0,0,0));
   innerShield = new G4SubtractionSolid("innerShield_2",innerShield,windowCut_dn          ,0,Pw_dn   );
   innerShield = new G4SubtractionSolid("innerShield_3",innerShield,windowCut_beamLeft_dn ,0,Pw_bl_dn);
   innerShield = new G4SubtractionSolid("innerShield_4",innerShield,windowCut_beamRight_dn,0,Pw_br_dn);
   innerShield = new G4SubtractionSolid("innerShield"  ,innerShield,windowCut_up          ,0,Pw_up   );

   // accumulate into logical volume pointer 
   G4LogicalVolume **logicShield = new G4LogicalVolume*[2]; 
   logicShield[0] = new G4LogicalVolume(innerShield,GetMaterial("Carbon_Steel_1008"),"logicGEnTarget_InnerShield");
   logicShield[1] = new G4LogicalVolume(outerShield,GetMaterial("Carbon_Steel_1008"),"logicGEnTarget_OuterShield");

   G4VisAttributes *vis = new G4VisAttributes();
   vis->SetColour( G4Colour::Magenta() );
   vis->SetForceWireframe(true);

   // rotation angle 
   G4double RY = 55.0*deg;  // FIXME: This angle is still an estimate!  

   bool isBoolean     = true;
   bool checkOverlaps = true;

   char physName[200]; 

   // placement 
   for(int i=0;i<2;i++){
      // visualization 
      logicShield[i]->SetVisAttributes(vis);
      // rotation
      G4RotationMatrix *rm = new G4RotationMatrix();
      rm->rotateX(0.*deg); rm->rotateY(RY); rm->rotateZ(0.*deg);
      // physical volume name
      if(i==0) sprintf(physName,"physGEnTarget_InnerShield"); 
      if(i==1) sprintf(physName,"physGEnTarget_OuterShield");
      G4ThreeVector P = G4ThreeVector(0,0,0);  
      // placement 
      new G4PVPlacement(rm,                   // rotation relative to mother       
	                P,                    // position relative to mother         
	                logicShield[i],       // logical volume        
	                physName,             // physical volume name           
	                motherLog,            // logical mother     
	                isBoolean,            // is boolean device? (true or false)    
	                i,                    // copy number    
	                checkOverlaps);       // check overlaps  
      // register with DetectorConstruction object
      fDetCon->InsertTargetVolume( logicShield[i]->GetName() );  
   }

}

void G4SBSTargetBuilder::BuildGEnTarget_PickupCoils(G4LogicalVolume *motherLog){
   // Pickup coils that sit just below the GEn 3He target cell
   // - Drawing: All dimensions extracted from JT model from Bert Metzger (June 2020) 

   // global y offset: pickup coils sit 1.1" below the target cell (measured from top of coil mount)
   G4double inch = 2.54*cm; 
   G4double y0 = -1.1*inch - 1.5*cm;  // 1.5 cm accounts for center of coil mount 

   // coil mount, beam left 
   // ---- upstream 
   partParameters_t cbul; 
   cbul.name = "pu_coil_base"; cbul.shape = "box";
   cbul.r_tor = 0.0*mm; cbul.r_min = 0.0*mm; cbul.r_max = 0.0*mm; cbul.length = 0.0*mm;
   cbul.x_len = 6.0*mm; cbul.y_len = 48.0*mm; cbul.z_len = 67.0*mm;
   cbul.startTheta = 0.0*deg; cbul.dTheta = 0.0*deg;
   cbul.startPhi = 0.0*deg; cbul.dPhi = 0.0*deg;
   cbul.x = 0.0*mm; cbul.y = -24.0*mm; cbul.z = 0.0*mm;
   cbul.rx = 0.0*deg; cbul.ry = 0.0*deg; cbul.rz = 0.0*deg;

   G4Box *coilB = new G4Box("coilB",cbul.x_len/2.,cbul.y_len/2.,cbul.z_len/2.);

   // U-cut: create a *subtraction* with a component for a U shape  
   G4double xlen = 2.*cbul.x_len;  // far exceed the thickness to make sure it's a cutaway 
   G4double ylen = 2.*1.3*cm; 
   G4double zlen = 3.2*cm; 

   G4Box *Ucut = new G4Box("Ucut",xlen/2.,ylen/2.,zlen/2.);

   // do the subtraction 
   G4ThreeVector Psub = G4ThreeVector(0,1.1*cm+ylen/2.,0.);  // centers the second volume relative to the first 
   G4SubtractionSolid *coilBase = new G4SubtractionSolid("coilMount",coilB,Ucut,0,Psub);

   // coil 
   partParameters_t pu; 
   pu.name = "pu_coil"; pu.shape = "box";
   pu.r_tor = 0.0*mm; pu.r_min = 0.0*mm; pu.r_max = 0.0*mm; pu.length = 0.0*mm;
   pu.x_len = 4.0*mm; pu.y_len = 30.0*mm; pu.z_len = 120.0*mm;
   pu.startTheta = 0.0*deg; pu.dTheta = 0.0*deg;
   pu.startPhi = 0.0*deg; pu.dPhi = 0.0*deg;
   pu.x = 0.0*mm; pu.y = 15.0*mm; pu.z = 0.0*mm;
   pu.rx = 0.0*deg; pu.ry = 0.0*deg; pu.rz = 0.0*deg;

   G4Box *coilOuter = new G4Box("coilOuter",pu.x_len/2.,pu.y_len/2.,pu.z_len/2.);

   // create a cutaway that actually defines the coil since the initial dimensions are the OUTER values 
   G4double xc = 2.*pu.x_len; // cut straight through in this dimension  
   G4double yc = pu.y_len - 2.*0.5*cm;
   G4double zc = pu.z_len - 2.*0.5*cm;

   G4Box *coilCut = new G4Box("coilCut",xc,yc/2.,zc/2.);

   // subtraction; no coordinates needed since we're centered on the outer coil  
   G4SubtractionSolid *coil = new G4SubtractionSolid("coil",coilOuter,coilCut);

   // coil mount 
   partParameters_t cmul;
   cmul.name = "pu_coil_mnt"; cmul.shape = "box";
   cmul.r_tor = 0.0*mm; cmul.r_min = 0.0*mm; cmul.r_max = 0.0*mm; cmul.length = 0.0*mm;
   cmul.x_len = 8.0*mm; cmul.y_len = 30.0*mm; cmul.z_len = 120.0*mm;
   cmul.startTheta = 0.0*deg; cmul.dTheta = 0.0*deg;
   cmul.startPhi = 0.0*deg; cmul.dPhi = 0.0*deg;
   cmul.x = 0.0*mm; cmul.y = 15.0*mm; cmul.z = 0.0*mm;
   cmul.rx = 0.0*deg; cmul.ry = 0.0*deg; cmul.rz = 0.0*deg;

   G4Box *coilMnt = new G4Box("cmnt",cmul.x_len/2.,cmul.y_len/2.,cmul.z_len/2.);

   // use the coil to cut its shape into the coil mount
   // need to locate the coil properly
   xc = pu.x_len/2.;
   yc = 0.*cm;
   zc = 0.*cm;
   G4ThreeVector Pc = G4ThreeVector(xc,yc,zc);
   G4SubtractionSolid *coilMount = new G4SubtractionSolid("coilMount",coilMnt,coil,0,Pc); 
  
   // union of coil base and mount 
   // now create a union of the coil base and coil mount
   G4double xm = cbul.x_len/2. + cmul.x_len/2.;
   G4double ym = 0.9*cm;
   G4double zm = 0.*cm;
   G4ThreeVector Pm = G4ThreeVector(xm,ym,zm);
   G4UnionSolid *coilBMNT = new G4UnionSolid("coilBMNT",coilBase,coilMount,0,Pm);

   G4VisAttributes *vis = new G4VisAttributes();
   vis->SetColour( G4Colour::Magenta() );
   // vis->SetForceWireframe(true);  

   bool isBoolean     = true; 
   bool checkOverlaps = true;  

   // FIXME: constraints come from JT file; is the 2.5 cm correct?
   G4double xbm = (2.5*cm)/2. + cbul.x_len/2. +  cmul.x_len + pu.x_len;
   G4double ybm = -0.9*cm + y0; // placement relative to BEAM 
   G4double zbm = 7.2*cm;

   // logical volumes
   G4LogicalVolume **logicPUCoilMount = new G4LogicalVolume*[4]; 

   char physName[200],logicName[200]; 

   // place the mounts 
   G4double X=0,Y=0,Z=0;
   G4double RX=0,RY=0,RZ=0;
   for(int i=0;i<4;i++){
      sprintf(logicName,"logicGEnTarget_PUCoilMNT_%d",i); 
      logicPUCoilMount[i] = new G4LogicalVolume(coilBMNT,GetMaterial("Aluminum"),logicName);
      logicPUCoilMount[i]->SetVisAttributes(vis);
      if(i==0){
         // upstream, beam left
         X = xbm; Y = ybm; Z = zbm;
         RY = 180.*deg;
      }else if(i==1){
         // upstream, beam right
         X = -xbm; Y = ybm; Z = zbm;
         RY = 0.*deg;
      }else if(i==2){
         // downstream, beam left
         X = xbm; Y = ybm; Z = -zbm;
         RY = 180.*deg;
      }else if(i==3){
         // downstream, beam right 
         X = -xbm; Y = ybm; Z = -zbm;
         RY = 0.*deg;
      }
      G4RotationMatrix *rm = new G4RotationMatrix();
      rm->rotateX(RX); rm->rotateY(RY); rm->rotateZ(RZ);
      // physical name 
      sprintf(physName,"physGEnTarget_PUCoilMNT_%d",i); 
      new G4PVPlacement(rm,                   // rotation [relative to mother]             
                        G4ThreeVector(X,Y,Z), // position [relative to mother]          
                        logicPUCoilMount[i],  // logical volume                            
                        physName,             // name                                       
                        motherLog,            // logical mother volume                   
                        isBoolean,            // boolean operations (true, false) 
                        i,                    // copy number                          
                        checkOverlaps);       // check overlaps                         
      // register with DetectorConstruction object 
      fDetCon->InsertTargetVolume( logicPUCoilMount[i]->GetName() );  
   }

   // place the pickup coils 

   // logical volumes
   G4LogicalVolume **logicPUCoil = new G4LogicalVolume*[4]; 

   G4VisAttributes *visCoil = new G4VisAttributes();
   visCoil->SetColour( G4Colour(255,140,0) );  // dark orange

   // place it to be flush against the mount assembly 
   G4double xbm_c = xbm - pu.x_len/2. - cmul.x_len;
   G4double ybm_c = y0;
   G4double zbm_c = zbm;

   // place the coils 
   for(int i=0;i<4;i++){
      sprintf(logicName,"logicGEnTarget_PUCoil_%d",i); 
      logicPUCoil[i] = new G4LogicalVolume(coil,GetMaterial("Copper"),logicName);
      logicPUCoil[i]->SetVisAttributes(visCoil);
      if(i==0){
	 // upstream, beam left
	 X = xbm_c; Y = ybm_c; Z = zbm_c;
	 RY = 180.*deg;
      }else if(i==1){
	 // upstream, beam right
	 X = -xbm_c; Y = ybm_c; Z = zbm_c;
	 RY = 0.*deg;
      }else if(i==2){
	 // downstream, beam left
	 X = xbm_c; Y = ybm_c; Z = -zbm_c;
	 RY = 180.*deg;
      }else if(i==3){
	 // downstream, beam right 
	 X = -xbm_c; Y = ybm_c; Z = -zbm_c;
	 RY = 0.*deg;
      }
      G4RotationMatrix *rm = new G4RotationMatrix();
      rm->rotateX(RX); rm->rotateY(RY); rm->rotateZ(RZ);
      // physical name 
      sprintf(physName,"physGEnTarget_PUCoil_%d",i); 
      new G4PVPlacement(rm,                   // rotation [relative to mother]             
	                G4ThreeVector(X,Y,Z), // position [relative to mother]          
	                logicPUCoil[i],       // logical volume                            
	                physName,             // name                                       
	                motherLog,            // logical mother volume                   
	                isBoolean,            // boolean operations (true, false) 
	                i,                    // copy number                          
	                checkOverlaps);       // check overlaps                        
      // register with DetectorConstruction object 
      fDetCon->InsertTargetVolume( logicPUCoil[i]->GetName() );  
   }

}

void G4SBSTargetBuilder::BuildGEnTarget_LadderPlate(G4LogicalVolume *motherLog){
   // Ladder plate for GEn 3He target
   // - Drawing number: A09016-03-04-0601

   // global offsets from JT model
   G4double inch = 25.4*mm;  
   G4double x0   = -0.438*inch; // beam right   
   G4double y0   = -5.33*cm;    // lower than target cell  
   G4double z0   =  1.0;        // TODO: should be 1.5*inch, but I see overlaps in stand-alone build

   // vertical posts along the y axis 
   // ---- upstream 
   partParameters_t lvu; 
   lvu.name = "ladder_vert_up"; lvu.shape = "box";
   lvu.r_tor = 0.0*mm; lvu.r_min = 0.0*mm; lvu.r_max = 0.0*mm; lvu.length = 0.0*mm;
   lvu.x_len = 12.7*mm; lvu.y_len = 203.2*mm; lvu.z_len = 38.1*mm;
   lvu.startTheta = 0.0*deg; lvu.dTheta = 0.0*deg;
   lvu.startPhi = 0.0*deg; lvu.dPhi = 0.0*deg;
   lvu.x = -11.1*mm; lvu.y = 0.0*mm; lvu.z = -363.0*mm;
   lvu.rx = 0.0*deg; lvu.ry = 0.0*deg; lvu.rz = 0.0*deg;

   G4Box *ladder_vert_up    = new G4Box("lvu",lvu.x_len/2.,lvu.y_len/2.,lvu.z_len/2.);
   G4ThreeVector P_lvu      = G4ThreeVector(lvu.x,lvu.y,lvu.z);
   G4RotationMatrix *rm_lvu = new G4RotationMatrix();
   rm_lvu->rotateX(lvu.rx); rm_lvu->rotateY(lvu.ry); rm_lvu->rotateZ(lvu.rz);

   // ---- downstream
   partParameters_t lvd; 
   lvd.name = "ladder_vert_dn"; lvd.shape = "box";
   lvd.r_tor = 0.0*mm; lvd.r_min = 0.0*mm; lvd.r_max = 0.0*mm; lvd.length = 0.0*mm;
   lvd.x_len = 12.7*mm; lvd.y_len = 203.2*mm; lvd.z_len = 38.1*mm;
   lvd.startTheta = 0.0*deg; lvd.dTheta = 0.0*deg;
   lvd.startPhi = 0.0*deg; lvd.dPhi = 0.0*deg;
   lvd.x = 0.0*mm; lvd.y = 0.0*mm; lvd.z = 363.0*mm;
   lvd.rx = 0.0*deg; lvd.ry = 0.0*deg; lvd.rz = 0.0*deg;

   G4Box *ladder_vert_dn    = new G4Box("lvd",lvd.x_len/2.,lvd.y_len/2.,lvd.z_len/2.);
   G4ThreeVector P_lvd      = G4ThreeVector(lvd.x,lvd.y,lvd.z);
   G4RotationMatrix *rm_lvd = new G4RotationMatrix();
   rm_lvd->rotateX(lvd.rx); rm_lvd->rotateY(lvd.ry); rm_lvd->rotateZ(lvd.rz);

   // horizontal posts along the z axis 
   // ---- above
   partParameters_t la;  
   la.name = "ladder_above"; la.shape = "box";
   la.r_tor = 0.0*mm; la.r_min = 0.0*mm; la.r_max = 0.0*mm; la.length = 0.0*mm;
   la.x_len = 12.7*mm; la.y_len = 38.1*mm; la.z_len = 370.0*mm;
   la.startTheta = 0.0*deg; la.dTheta = 0.0*deg;
   la.startPhi = 0.0*deg; la.dPhi = 0.0*deg;
   la.x = 0.0*mm; la.y = 226.4*mm; la.z = 314.2*mm;
   la.rx = 0.0*deg; la.ry = 0.0*deg; la.rz = 0.0*deg;

   G4Box *ladder_above     = new G4Box("la",la.x_len/2.,la.y_len/2.,la.z_len/2.);
   G4ThreeVector P_la      = G4ThreeVector(la.x,la.y,la.z);
   G4RotationMatrix *rm_la = new G4RotationMatrix();
   rm_la->rotateX(la.rx); rm_la->rotateY(la.ry); rm_la->rotateZ(la.rz);

   // ---- below 
   partParameters_t lb; 
   lb.name = "ladder_below"; lb.shape = "box";
   lb.r_tor = 0.0*mm; lb.r_min = 0.0*mm; lb.r_max = 0.0*mm; lb.length = 0.0*mm;
   lb.x_len = 12.7*mm; lb.y_len = 38.1*mm; lb.z_len = 114.3*mm;
   lb.startTheta = 0.0*deg; lb.dTheta = 0.0*deg; 
   lb.startPhi = 0.0*deg; lb.dPhi = 0.0*deg;
   lb.x = 0.0*mm; lb.y = -158.3*mm; lb.z = 314.2*mm;
   lb.rx = 0.0*deg; lb.ry = 0.0*deg; lb.rz = 0.0*deg;

   G4Box *ladder_below     = new G4Box("lb",lb.x_len/2.,lb.y_len/2.,lb.z_len/2.);
   G4ThreeVector P_lb      = G4ThreeVector(lb.x,lb.y,lb.z);
   G4RotationMatrix *rm_lb = new G4RotationMatrix();
   rm_lb->rotateX(lb.rx); rm_lvd->rotateY(lb.ry); rm_lb->rotateZ(lb.rz);

   // angular part above, along z axis
   // ---- upstream
   partParameters_t aau;  
   aau.name = "ladder_ang_above_up"; aau.shape = "box";
   aau.r_tor = 0.0*mm; aau.r_min = 0.0*mm; aau.r_max = 0.0*mm; aau.length = 0.0*mm;
   aau.x_len = 12.7*mm; aau.y_len = 38.1*mm; aau.z_len = 240.0*mm;
   aau.startTheta = 0.0*deg; aau.dTheta = 0.0*deg; 
   aau.startPhi = 0.0*deg; aau.dPhi = 0.0*deg;
   aau.x = 0.0*mm; aau.y = 150.0*mm; aau.z = 70.4*mm;
   aau.rx = 45.0*deg; aau.ry = 0.0*deg; aau.rz = 0.0*deg;

   G4Box *ladder_aau        = new G4Box("aau",aau.x_len/2.,aau.y_len/2.,aau.z_len/2.);
   G4ThreeVector P_aau      = G4ThreeVector(aau.x,aau.y,aau.z);
   G4RotationMatrix *rm_aau = new G4RotationMatrix();
   rm_aau->rotateX(aau.rx); rm_aau->rotateY(aau.ry); rm_aau->rotateZ(aau.rz); 
   
   // ---- downstream
   partParameters_t aad;
   aad.name = "ladder_ang_above_dn"; aad.shape = "box";
   aad.r_tor = 0.0*mm; aad.r_min = 0.0*mm; aad.r_max = 0.0*mm; aad.length = 0.0*mm;
   aad.x_len = 12.7*mm; aad.y_len = 38.1*mm; aad.z_len = 280.0*mm;
   aad.startTheta = 0.0*deg; aad.dTheta = 0.0*deg; 
   aad.startPhi = 0.0*deg; aad.dPhi = 0.0*deg;
   aad.x = 0.0*mm; aad.y = 150.0*mm; aad.z = 600.0*mm;
   aad.rx = -36.0*deg; aad.ry = 0.0*deg; aad.rz = 0.0*deg;

   G4Box *ladder_aad        = new G4Box("aad",aad.x_len/2.,aad.y_len/2.,aad.z_len/2.);
   G4ThreeVector P_aad      = G4ThreeVector(aad.x,aad.y,aad.z);
   G4RotationMatrix *rm_aad = new G4RotationMatrix();
   rm_aad->rotateX(aad.rx); rm_aad->rotateY(aad.ry); rm_aad->rotateZ(aad.rz);

   // angular part below, along z axis 
   // ---- upstream 
   partParameters_t abu;
   abu.name = "ladder_ang_below_up"; abu.shape = "box";
   abu.r_tor = 0.0*mm; abu.r_min = 0.0*mm; abu.r_max = 0.0*mm; abu.length = 0.0*mm;
   abu.x_len = 12.7*mm; abu.y_len = 38.1*mm; abu.z_len = 282.0*mm;
   abu.startTheta = 0.0*deg; abu.dTheta = 0.0*deg;
   abu.startPhi = 0.0*deg; abu.dPhi = 0.0*deg;
   abu.x = 0.0*mm; abu.y = -125.0*mm; abu.z = 131.0*mm;
   abu.rx = -14.4*deg; abu.ry = 0.0*deg; abu.rz = 0.0*deg;

   G4Box *ladder_abu        = new G4Box("abu",abu.x_len/2.,abu.y_len/2.,abu.z_len/2.);
   G4ThreeVector P_abu      = G4ThreeVector(abu.x,abu.y,abu.z);
   G4RotationMatrix *rm_abu = new G4RotationMatrix();
   rm_abu->rotateX(abu.rx); rm_abu->rotateY(abu.ry); rm_abu->rotateZ(abu.rz);

   // ---- downstream
   partParameters_t abd; 
   abd.name = "ladder_ang_below_dn"; abd.shape = "box";
   abd.r_tor = 0.0*mm; abd.r_min = 0.0*mm; abd.r_max = 0.0*mm; abd.length = 0.0*mm;
   abd.x_len = 12.7*mm; abd.y_len = 38.1*mm; abd.z_len = 350.0*mm;
   abd.startTheta = 0.0*deg; abd.dTheta = 0.0*deg;
   abd.startPhi = 0.0*deg; abd.dPhi = 0.0*deg;
   abd.x = 0.0*mm; abd.y = -125.0*mm; abd.z = 540.0*mm;
   abd.rx = 11.3*deg; abd.ry = 0.0*deg; abd.rz = 0.0*deg;

   G4Box *ladder_abd        = new G4Box("abd",abd.x_len/2.,abd.y_len/2.,abd.z_len/2.);
   G4ThreeVector P_abd      = G4ThreeVector(abd.x,abd.y,abd.z);
   G4RotationMatrix *rm_abd = new G4RotationMatrix();
   rm_abd->rotateX(abd.rx); rm_abd->rotateY(abd.ry); rm_abd->rotateZ(abd.rz); 

   // union solid 
   G4UnionSolid *ladder;
   // [downstream] pane inner and outer  
   G4ThreeVector P     = G4ThreeVector(0.,0.,lvd.z*2.);
   ladder = new G4UnionSolid("lud",ladder_vert_up,ladder_vert_dn,0,P);
   // [above] horizontal 
   G4ThreeVector P_a   = G4ThreeVector(0.,la.y,la.z);
   ladder = new G4UnionSolid("lud_a" ,ladder,ladder_above,0,P_a);
   // [below] horizontal 
   G4ThreeVector P_b   = G4ThreeVector(0.,lb.y,lb.z);
   ladder = new G4UnionSolid("lud_ab",ladder,ladder_below,0,P_b);
   // [above] angled above, upstream  
   ladder = new G4UnionSolid("lud_ab_aau",ladder,ladder_aau,rm_aau,P_aau);
   // [above] angled below, downstream  
   ladder = new G4UnionSolid("lud_ab_aaud",ladder,ladder_aad,rm_aad,P_aad);
   // [below] angled below, upstream  
   ladder = new G4UnionSolid("lud_ab_aaud_abu",ladder,ladder_abu,rm_abu,P_abu);
   // [below] angled, downstream  
   ladder = new G4UnionSolid("ladder",ladder,ladder_abd,rm_abd,P_abd);

   G4VisAttributes *vis = new G4VisAttributes();
   vis->SetColour( G4Colour::Red() );
   // vis->SetForceWireframe(true);

   G4LogicalVolume *logicLadder = new G4LogicalVolume(ladder,GetMaterial("Ultem"),"logicGEnTarget_Ladder");
   logicLadder->SetVisAttributes(vis);

   // placement 
   G4double lx = x0;
   G4double ly = y0;
   G4double lz = lvu.z + z0;

   G4ThreeVector P_l      = G4ThreeVector(lx,ly,lz);
   G4RotationMatrix *rm_l = new G4RotationMatrix();
   rm_l->rotateX(0.*deg); rm_l->rotateY(0.*deg); rm_l->rotateZ(0.*deg);

   bool isBoolean     = true;
   bool checkOverlaps = true;

   new G4PVPlacement(rm_l,                // rotation [relative to mother]    
                     P_l,                 // position [relative to mother] 
                     logicLadder,         // logical volume    
                     "physLadder",        // name                          
                     motherLog,         // logical mother volume            
                     isBoolean,           // boolean operations          
                     0,                   // copy number                   
                     checkOverlaps);     // check overlaps  

   // register with DetectorConstruction object
   fDetCon->InsertTargetVolume( logicLadder->GetName() );  

}

