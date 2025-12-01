// FireLord Device Case

$fn = 32;

solar_panel_len = 160.4;
solar_panel_wid = 138.4;
solar_panel_h = 2.7;

perfboard_len = 90.4;
perfboard_wid = 70.4;
perfboard_thick = 1.6;
perfboard_leeway = 6.0; // spacing under the board
perfboard_hole_dia = 3.0;
// inset distances from each perboard corner
perfboard_hole_corner_x = 1.8+perfboard_hole_dia/2;
perfboard_hole_corner_y = 0.8+perfboard_hole_dia/2;

supercap_dia = 36.1;
supercap_len = 73.3;

lora_screw_shaft_dia = 1.9;
lora_screw_shaft_len = 6.1;
lora_screw_head_dia = 4.0;
lora_screw_head_len = 2.1;
// The LoRa module has three mounting holes in an
// isosceles triangle layout (if they're your vertices)
// These are the holes closest together (the base)
lora_hole_tria_base = 21.8-lora_screw_shaft_dia; // From far hole edge to far hole edge; -dia makes centered.
lora_hole_tria_h = 29.0+lora_screw_shaft_dia/2;
lora_antenna_hole_dia = 2.0;
// Offsets from top-most mount hole to antenna hole
lora_antenna_h_offset_x = -3.7;
lora_antenna_h_offset_y = -2.6;

wall_thick = 1.6;

supercap_clip_h = supercap_dia/6; // How far the clipping portion extends
cap_clip_rounding = 0.7;
cap_clip_section_thick = 4.0;

case_rounding = 4.0;

case_screw_dia = 3.0;

module loraScrewHole2D() {
    circle(d=lora_screw_shaft_dia);
}

module loraScrewHoles2D() {
    translate([-lora_hole_tria_base/2,0])
    circle(d=lora_screw_shaft_dia);
    translate([lora_hole_tria_base/2,0])
    circle(d=lora_screw_shaft_dia);
    translate([0,lora_hole_tria_h]) {
        circle(d=lora_screw_shaft_dia);
        //translate([lora_antenna_h_offset_x,lora_antenna_h_offset_y])
        //circle(d=lora_antenna_hole_dia);
    }
}

module perfboardHole2D() {
    circle(d=perfboard_hole_dia);
}

module perfboardHoles2D() {
    x_val = perfboard_len/2-perfboard_hole_corner_x;
    y_val = perfboard_wid/2-perfboard_hole_corner_y;

    translate([x_val, y_val])
    perfboardHole2D();
    translate([-x_val, y_val])
    perfboardHole2D();
    translate([x_val, -y_val])
    perfboardHole2D();
    translate([-x_val, -y_val])
    perfboardHole2D();
}
//perfboardHoles2D();

module capacitor2D() {
    circle(d=supercap_dia);
}

module capacitorClipDiff2D() {
    difference() {
        capacitor2D();
        translate([0,supercap_dia/2+supercap_clip_h])
        square(supercap_dia, center=true);
    }
}
//capacitorClipDiff2D();

// Side profile
module capacitorClipCircle2D() {
    difference() {
        offset(wall_thick)
        capacitorClipDiff2D();
        translate([0,supercap_dia/2+supercap_clip_h])
        square([supercap_dia+wall_thick*2, supercap_dia], center=true);
    }
}
//capacitorClipCircle2D();

module capacitorClip2D() {
    offset(cap_clip_rounding) offset(-cap_clip_rounding)
    difference() {
        hull() {
            capacitorClipCircle2D();
            translate([0,-supercap_dia/4+supercap_clip_h/2])
            square([supercap_dia+3*wall_thick, supercap_dia/2+supercap_clip_h+wall_thick*2], center=true);
        }
        capacitor2D();
    }
    translate([0,cap_clip_rounding/2-supercap_dia/2-wall_thick])
    square([supercap_dia+3*wall_thick, cap_clip_rounding], center=true);
}
//capacitorClip2D();
// Test translate of the screw hole
//translate([supercap_dia/2+perfboard_hole_dia/2+wall_thick,0])
//#square([perfboard_hole_dia, 10], center=true);

module capacitorClipSection3D() {
    translate([0,cap_clip_section_thick/2,supercap_dia/2])
    rotate([90,0,0])
    linear_extrude(cap_clip_section_thick)
    capacitorClip2D();
}
capacitorClipSection3D();

//module caseScrewHoles

module caseBodyHollow() {
    difference() {
        linear_extrude(supercap_dia*2)
        offset(case_rounding) offset(-case_rounding)
        square([solar_panel_len+4*wall_thick+2*case_screw_dia, solar_panel_wid+4*wall_thick+2*case_screw_dia], center=true);
        translate([0,0,wall_thick])
        linear_extrude(supercap_dia*2-wall_thick)
        offset(case_rounding) offset(-case_rounding)
        square([solar_panel_len, solar_panel_wid], center=true);
    }
}

//caseBodyHollow();