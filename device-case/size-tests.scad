// FireLord Device Case

use <case.scad>;


$fn = 32;

module holeTestifyHelper() {
    difference() {
        offset(1.6)
        hull() {
            children();
        }
        children();
    }
}

module holeTestify() {
    linear_extrude(1.0)
    difference() {
        holeTestifyHelper() children();
        
        offset(1.6) offset(-1.6*2)
        holeTestifyHelper() children();
    }
}

// To be printed in vase mode
module capacitorTest3D() {
    linear_extrude(73.3+0.4)
    offset(0.4)
    capacitor2D();
    
    
}
//capacitorTest3D();

// Small prints to test if the hole spacing/sizes are correct
//holeTestify() loraScrewHoles2D();
holeTestify() perfboardHoles2D();