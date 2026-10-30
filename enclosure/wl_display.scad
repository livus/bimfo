// Wiener Linien "Abfahrt in Minuten" stop display — desk replica
// Enclosure for the Waveshare ESP32-S3-LCD-1.47B (1.47" 172x320 LCD, landscape).
//
// Modelled on the classic grey AEG passenger-information box found at Vienna
// tram/bus stops: grey housing, dark window, "Linie" / "Abfahrt in Minuten"
// printed above the window, two screws below it, held to a round pole by two
// band brackets, with two horn loudspeakers behind the pole under the red cap.
//
// Coordinates (display view): X right, Y into the box (front face at y=0),
// Z up. The LCD active area is centred on the origin.
//
// Render a single part with  -D 'part="shell"'  etc. (see README.md).

/* [Output] */
part = "assembly"; // [assembly, shell, back, back_labels, pole, pole_cover, pole_top, speaker_bracket, horn, cap, base, base_strip, fence, fence_half, can, labels, screen_dummy, fittest, cabletest, cabletest_pole, cabletest_cover]
// Side the pole (and the USB cable) is on. For "left", enable "Anzeige um 180° drehen" in the settings page.
pole_side = "right"; // [right, left]

/* [Board — measure these if the fit test is off] */
pcb_l = 36.37;          // Waveshare drawing
pcb_w = 20.32;          // Waveshare drawing
pcb_t = 1.2;
aa_w = 32.35;           // LCD active area (Waveshare spec)
aa_h = 17.39;
aa_dx = 0;              // active-area centre offset from PCB centre, + = towards USB
aa_dz = 0;              // + = towards RST edge (front view, USB to the right)
glass_to_pcb = 3.6;     // front of LCD glass to front face of PCB
standoff_h = 3.5;       // brass M2 standoffs on the component side
// Front of glass to tips of the brass standoffs: the one number worth measuring.
stack_h = glass_to_pcb + pcb_t + standoff_h;
usb_overhang = 1.5;     // USB-C receptacle sticks out past the PCB edge
usb_h = 3.26;
btn_u = 29.5;           // BOOT/RST actuators, distance from the antenna end
btn_protrude = 1.6;     // actuator tip beyond the PCB edge
btn_h = 0.8;            // actuator centre above the PCB back face
// Brass standoffs [u from antenna end, v from BOOT edge] (front view)
standoffs = [[1.97, 3.52], [1.97, 16.80], [33.97, 2.00], [33.97, 18.32]];
// Soldered-in pin headers: two rows of 9 at 2.54 mm pitch along the long edges,
// 17.78 mm apart, last pin 11.31 mm from the USB end (Waveshare drawing).
header_pins = true;     // cut slits for them in the back cover
header_u = [4.74, 25.06];   // first and last pin centre from the antenna end
header_v = 1.27;        // row centre from each long edge
header_body = 2.5;      // plastic spacer on the component side
header_len = 8.5;       // PCB back face to the pin tips (standard header: 2.5 + 6)
header_slit_w = 1.3;    // slit in the back cover for each row (pins are 0.64 mm square)

/* [Housing] */
// Frame beside the window on the free (antenna) side. The pole side is as thin as
// the board allows; like the original, that side's frame is the narrower one.
margin_free = 5;
wall_pole = 1.2;        // box wall on the pole side, past the PCB edge
gap = 1.8;              // visible gap between box and pole, closed by a filler plate between the brackets
filler_recess = 1.2;    // filler plate sits this far behind the box face
wall = 1.6;
front_t = 1.2;          // bezel thickness in front of the glass
depth = 13;
win_margin = 0.5;       // window overlap beyond the active area (shows black glass)
// Fit test: the visible screen sits ~1.1 mm further towards the antenna end than the
// drawing suggests. Each window edge moves on its own; the board and box stay put.
win_dx = [-1.2, -1.0];  // shift of the [antenna-side, USB-side] window edge, + = towards USB
btn_gap = 0.2;          // actuator tip to inner wall
fit = 0.1;              // clearance for the back cover
boot_nub = 0.1;         // nub on the BOOT flexure; raise if the press does nothing
label_size = 1.8;       // ~1.7 mm caps; both labels fit over the window width
label_depth = 0.4;      // two 0.2 mm layers
label_font = "Liberation Sans:style=Bold";

/* [USB plug — 90° side-angle USB-C or soldered 2-wire lead] */
plug_depth = 11;        // head length measured from the receptacle face
plug_up = 6.5;          // head extent above the plug centreline
plug_down = 11;         // rigid head extent below the centreline (boot goes into pole)
plug_thick = 7.2;
plug_clr = 0.4;
boot_d = 6.2;           // strain-relief boot below the head (clearance included)
boot_len = 7;
boot_offset = 5.5;      // receptacle face to the centre of the boot, along the plug
cable_d = 4.4;

/* [Pole, brackets and base] */
pole_d = 12;
pole_bore = 9.0;
pole_flat = 7;          // flat on the back of the pole: print bed contact + rotation key
slot_w = cable_d + 0.2; // rear slot to lay the cable into the pole
cover_lip = 0.15;       // the pole cover's lip reaches this far behind each slot edge
pole_below = 110;       // visible pole between box and pavement (~1:15 would be 140)
pole_tongue = 6;        // the two nibs on the pole top that slot up into the lower bracket
band_t = 0.6;           // how far the bracket bands stand proud of the pole
// Bracket heights as fractions of the box height: each about a quarter of the
// box, flush with its top and bottom, as on the original
upper_bracket = [0.25, 0.5];    // [bottom, top] in units of H around the centre
lower_bracket_top = -0.25;
stub_h = 7.1;           // pole above the box, up to the cap
socket_depth = 6;       // pole in the base
base_h = 12;
curb_drop = 3;          // platform above the track
// The base is a piece of platform with the track beside it, running at right
// angles to the display; the box reaches out from the pole towards the track.
// Asphalt platform with flush concrete edge stones along the track; the track
// lies in concrete slabs, with a narrow strip along the foot of the curb. All of
// it is drawn by joints, so it prints in one colour. Compressed: at true scale
// the near rail alone would be ~45 mm from the curb.
plat_edge_gap = 3;      // platform edge beyond the box's free end
road_w = 22;            // track strip beside the platform
rail_off = 7;           // curb face to the near rail's head
edge_w = 7;             // platform edge stones on the track side
block_l = 26.8;         // edge stone length
joint_w = 0.5;          // joints between blocks and slabs
joint_d = 0.4;

/* [Railing and Mistkuebel] */
// On the roadway side of the platform a plain galvanised tube railing (one tube
// bent into an inverted U, plus a lower rail), with the city's grey-and-orange
// litter bin hung on its platform side, as at Vienna's tram stops. The bin has
// the ash tube for cigarettes at its rear corner. About 1:15.
fence_d = 3;            // railing tube (~45 mm)
fence_h = 58;           // top rail above the platform (~0.9 m)
fence_r = 6;            // bend at the ends of the top rail
can_size = [24, 17, 43];        // bin width (along the railing), depth, height (~36 x 26 x 65 cm)
can_z0 = 26;            // bin bottom above the platform
can_band = [6, 13];     // the orange "Misttelefon" band, from the bin's bottom
fence_mid = can_z0 + 4; // lower rail: crosses the bin's back between flange and band, so the bin sits on both rails

/* [Speakers] */
// Fan-shaped plate under the cap with a horn loudspeaker under each outer corner.
spk_mouth = [9, 7];     // rectangular horn mouth, width x height
spk_len = 10;           // mouth to back of the driver can
plate_r = 11;           // pole axis to the plate corners
// Angles in degrees from "away from the box"; negative = towards the front.
// Symmetric, so the plate sits opposite the display and the horns face front and back.
spk_mount_angles = [-25, 25];   // where the horns are bolted on; the plate is sized around them
spk_tilt = 15;          // horns point slightly downwards

$fn = 48;
eps = 0.01;

// ---------------------------------------------------------------- derived
H = pcb_w + 2 * (btn_protrude + btn_gap + wall);   // housing height
D = depth;
boot_bottom = pole_side == "right";   // board rotated 180° for a left pole
adz = boot_bottom ? aa_dz : -aa_dz;

bx = -pcb_l / 2 - aa_dx;              // PCB antenna-end edge
bz = -pcb_w / 2 - adz;                // PCB bottom edge
x_rf = bx + pcb_l + usb_overhang;     // USB receptacle face
pcb_front = front_t + glass_to_pcb;
pcb_back = pcb_front + pcb_t;
usb_y = pcb_back + usb_h / 2;
usb_z = bz + pcb_w / 2;
btn_x = bx + btn_u;
btn_y = pcb_back + btn_h;
cav_z0 = bz - btn_protrude - btn_gap;
cav_z1 = bz + pcb_w + btn_protrude + btn_gap;
pad_h = 0.4;
cover_y = front_t + stack_h + pad_h;  // inner face of the back cover

win_w = aa_w + 2 * win_margin;
win_h = aa_h + 2 * win_margin;
// Window edges in display view (the USB end is on the left for a left pole)
win_x0 = boot_bottom ? -win_w / 2 + win_dx[0] : -win_w / 2 - win_dx[1];
win_x1 = boot_bottom ? win_w / 2 + win_dx[1] : win_w / 2 - win_dx[0];

head_x0 = x_rf - usb_overhang + 0.1;  // the receptacle passes through the thin pole-side wall
head_x1 = x_rf + plug_depth + plug_clr;
head_z0 = usb_z - plug_down - plug_clr;
head_z1 = usb_z + plug_up + plug_clr;
head_y0 = usb_y - plug_thick / 2 - plug_clr;
boot_x = x_rf + boot_offset;          // where the cable leaves the plug head

// Box outline: thin wall on the pole side, window off-centre towards the pole.
cav_x1 = bx + pcb_l + 0.25;           // board cavity end at the USB edge of the PCB
box_r = cav_x1 + wall_pole;
box_l = -win_w / 2 - margin_free;     // from the nominal window, so win_dx leaves the box alone
W = box_r - box_l;
// The pole stands a small gap away from the box. Its bore still has to take the
// plug's strain-relief boot, which comes down next to the USB port.
pole_x = box_r + gap + pole_d / 2;
pole_y = usb_y - 0.7;                 // a little forward, so the pole's front covers more of the plug head
// The filler plate reaches far enough along the pole to keep 1 mm of wall in front of the plug head.
filler_x1 = pole_x - sqrt(pow(pole_d / 2, 2) - pow(pole_y - head_y0 + 1, 2));
pole_r = pole_d / 2;
band_r = pole_r + band_t;
flat_y = sqrt(pole_r * pole_r - pole_flat * pole_flat / 4);   // axis to back flat

ub0 = upper_bracket[0] * H;
ub1 = upper_bracket[1] * H;           // = box top; the pole top stub sits here
lb1 = lower_bracket_top * H;
lb0 = -H / 2;                         // flush with the box bottom
pole_top_z = lb0;                     // the pole butts against the lower bracket
collar_top = ub1 + stub_h;            // top of the pole, where the cap sits
base_top = -H / 2 - pole_below;
plat_x0 = box_l - plat_edge_gap;      // platform edge (curb) on the track side
base_x0 = plat_x0 - road_w;
// The railing runs just behind the pole (seen from the platform); the bin hangs
// on it beside the pole, towards the front, and the base reaches forward for it.
fence_x = pole_x + pole_r + 1.5 + fence_d / 2;
base_x1 = fence_x + 3.5;
street_top = base_top - curb_drop;
can_y = pole_y - pole_r - 3 - can_size[0] / 2;
base_y0 = can_y - can_size[0] / 2 - 10;   // room for the railing's front bend beyond the bin
base_y1 = pole_y + 26;
base_slit = cable_d + 0.4;            // cable slit in the base, from the pole to the back edge
fence_y = [base_y0 + 4, base_y1 - 4];  // railing end posts
pole_z0 = base_top - socket_depth;

assert(cover_y > pcb_back + usb_h + 0.3, "back cover would hit the USB receptacle");
assert(!header_pins || cover_y > pcb_back + header_body + 0.2, "back cover would hit the pin header plastic");
assert(D - (head_y0 + plug_thick + 2 * plug_clr) >= 1.0, "increase depth for this plug");
assert(head_z0 > -H / 2 + 1.2 && head_z1 < ub1 - 5.2, "plug head does not fit the box height");
assert(norm([pole_x - boot_x, pole_y - usb_y]) + boot_d / 2 <= pole_d / 2 - 0.5, "gap too wide: the plug's boot no longer fits the pole");

// ---------------------------------------------------------------- helpers
module rrect(size, r) {   // 2D rounded rectangle, origin at the min corner
    offset(r) offset(-r) square(size);
}

module xz_prism(x0, x1, z0, z1, y0, y1) {
    translate([x0, y0, z0]) cube([x1 - x0, y1 - y0, z1 - z0]);
}

module z_extrude(z0, z1) {
    translate([0, 0, z0]) linear_extrude(z1 - z0) children();
}

// Pole cross-section: round at the front, flat at the back (prints on that flat,
// and the flat keys the pole's rotation in its sockets).
module pole_2d(r = pole_r) {
    intersection() {
        translate([pole_x, pole_y]) circle(r, $fn = 64);
        translate([pole_x - r - 1, pole_y - r - 1]) square([2 * r + 2, r + 1 + flat_y]);
    }
}

// Bore: the pole prints back-down, so the front of the bore is the ceiling
// and gets a capped teardrop. Rear slot to lay the cable in.
module bore_2d(sw = slot_w) {
    br = pole_bore / 2;
    intersection() {
        hull() {
            translate([pole_x, pole_y]) circle(br);
            translate([pole_x, pole_y - br * sqrt(2)]) rotate(45) square(eps, center = true);
        }
        translate([pole_x - br, pole_y - (pole_r - 1)]) square([2 * br, 2 * pole_r]);
    }
    translate([pole_x - sw / 2, pole_y]) square([sw, pole_r + 1]);
}

// Tongue on the pole top, on the side away from the plug head, that keys the
// pole into the lower bracket.
module tongue_2d() {
    intersection() {
        pole_2d(pole_r - 0.05);   // a hair inside the pole surface, so the two meshes never coincide
        translate([pole_x + 4.5, pole_y - pole_r]) square([pole_r, 2 * pole_r]);
    }
}

// Second nib, on the box side of the pole top, in front of the plug head. Above
// the pole end it may cover the bore (the cable comes down behind it). Its two
// back faces slope at 45 degrees, so it prints without overhang with the pole
// lying on its back.
module nib2_2d() {
    intersection() {
        pole_2d(pole_r - 0.05);
        translate([pole_x - 2.2, head_y0 - 0.8]) rotate(-135) square(20);
    }
}

// Boot pocket at the top of the pole: the boot is laid in from the back too.
module boot_pocket_2d() {
    hull() {
        translate([boot_x, usb_y]) circle(d = boot_d + 0.2);
        translate([boot_x - boot_d / 2 - 0.1, usb_y]) square([boot_d + 0.2, pole_r + 1]);
    }
    // open the wall behind the boot cleanly, so no knife-edge sliver is left
    translate([pole_x - pole_r - 1, usb_y]) square([boot_x - pole_x + pole_r + 1, pole_r + 1]);
}

module dowel_hole(x, z, y = pole_y) {   // a 7 mm piece of 1.75 mm filament
    translate([x, y, z]) cylinder(d = 2.0, h = 8, center = true, $fn = 16);
}

// The pole top stub's two dowels, placed clear of the speaker plate socket on its outer side
module stub_dowels() {
    dowel_hole(pole_x - 3, ub1);
    dowel_hole(pole_x, ub1, pole_y - 3);
}

// ---------------------------------------------------------------- shell
module box_outer() {
    ch = 0.5;
    hull() {
        // chamfer round the front and the back, as the double-sided original has;
        // at the front it also keeps the first layer from flaring
        for (y = [0, D - ch]) translate([0, y, 0]) rotate([-90, 0, 0])
            linear_extrude(ch) offset(-ch) translate([box_l, -H / 2]) rrect([W, H], 1.2);
        translate([0, ch, 0]) rotate([-90, 0, 0])
            linear_extrude(D - 2 * ch) translate([box_l, -H / 2]) rrect([W, H], 1.2);
    }
}

// Band bracket: flat front plate from the box side across the gap and the pole,
// band wrapped round the rest of the pole.
module bracket_2d() {
    hull() {
        translate([box_r - 1, 0]) square([pole_x + 3.5 - box_r + 1, eps]);
        translate([box_r - 1, 0]) square([eps, D]);
        translate([pole_x, pole_y]) circle(band_r, $fn = 64);
    }
}

// Flat plate closing the gap between box and pole, between the brackets. It
// hides the plug head and sits a little behind the bracket fronts.
module filler_2d() {
    // spans the gap and runs onto the pole only as far as the plug head needs
    translate([box_r - 1, filler_recess]) square([filler_x1 - box_r + 1, D - filler_recess]);
}

// Box side plus the two brackets, the filler plate and the short piece of pole
// between the brackets, which carries the plug head. The pole section is fully
// round (the back cover completes its back). The pole section and the filler
// print as bridges between the brackets.
module outer() {
    box_outer();
    z_extrude(ub0, ub1) bracket_2d();
    z_extrude(lb0, lb1) bracket_2d();
    z_extrude(lb1 - eps, ub0 + eps) { translate([pole_x, pole_y]) circle(pole_r, $fn = 64); filler_2d(); }
}

module board_cavity() {
    xz_prism(bx - 0.25, cav_x1, cav_z0, cav_z1, front_t, D + 1);
}

module head_cavity() {   // vertical edges rounded so the pole section keeps its wall
    z_extrude(head_z0, head_z1) offset(r = 1.5) offset(delta = -1.5)
        translate([head_x0, head_y0]) square([head_x1 - head_x0, D + 3 - head_y0]);
}

// Outline of the back opening (XZ), shared by the shell cut and the cover.
module cover_outline_2d(off = 0) {
    offset(delta = off) {
        translate([bx - 0.25, cav_z0]) square([cav_x1 - bx + 0.25, cav_z1 - cav_z0]);
        translate([head_x0, head_z0]) square([head_x1 - head_x0, head_z1 - head_z0]);
    }
}

module cover_region(off = 0) {
    translate([0, cover_y, 0]) rotate([-90, 0, 0]) mirror([0, 1, 0])
        linear_extrude(D + 3 - cover_y) cover_outline_2d(off);
}

module ribs() {   // locate the PCB edges away from the side buttons
    for (u = [6, 18]) {
        xz_prism(bx + u - 0.6, bx + u + 0.6, cav_z0 - eps, bz - 0.1, front_t, cover_y - 0.4);
        xz_prism(bx + u - 0.6, bx + u + 0.6, bz + pcb_w + 0.1, cav_z1 + eps, front_t, cover_y - 0.4);
    }
}

// Flexure in the wall under BOOT: U-slot, thinned from outside, nub inside.
module boot_flexure_cut() {
    l = 10; w = 4; s = 0.5;
    x0 = btn_x - l + 1.5; x1 = btn_x + 1.5;
    zw0 = boot_bottom ? -H / 2 - 1 : cav_z1 - eps;
    zw1 = boot_bottom ? cav_z0 + eps : H / 2 + 1;
    difference() {
        xz_prism(x0, x1 + s, zw0, zw1, btn_y - w / 2 - s, btn_y + w / 2 + s);
        xz_prism(x0 - 1, x1, zw0 - 1, zw1 + 1, btn_y - w / 2, btn_y + w / 2);
    }
    // thin the tab from outside
    if (boot_bottom) xz_prism(x0, x1, -H / 2 - 1, -H / 2 + 0.5, btn_y - w / 2 - eps, btn_y + w / 2 + eps);
    else xz_prism(x0, x1, H / 2 - 0.5, H / 2 + 1, btn_y - w / 2 - eps, btn_y + w / 2 + eps);
}

module boot_nub() {
    if (boot_bottom) xz_prism(btn_x - 0.8, btn_x + 0.8, cav_z0 - eps, cav_z0 + boot_nub, btn_y - 1, btn_y + 1);
    else xz_prism(btn_x - 0.8, btn_x + 0.8, cav_z1 - boot_nub, cav_z1 + eps, btn_y - 1, btn_y + 1);
}

module rst_hole() {
    z0 = boot_bottom ? cav_z1 - 1 : -H / 2 - 1;
    z1 = boot_bottom ? H / 2 + 1 : cav_z0 + 1;
    translate([btn_x, btn_y, z0]) cylinder(d = 1.4, h = z1 - z0, $fn = 16);
}

module snap_grooves(d = 0.35) {
    yc = (cover_y + D) / 2;
    for (z = [cav_z0 - d, cav_z1]) xz_prism(bx + 4, bx + 24, z, z + d, yc - 0.4, yc + 0.4);
}

module pry_notch() {
    xz_prism(bx - 1.5, bx + 0.1, -1.5, 1.5, D - 1, D + 1);
}

module cosmetic_grooves() {
    g = 0.3;
    // panel seam around the sides of the box, near the front and the back
    for (y = [2.3, D - 2.7]) difference() {
        xz_prism(box_l - 1, box_r, -H / 2 - 1, H / 2 + 1, y, y + 0.4);
        xz_prism(box_l + g, box_r + 1, -H / 2 + g, H / 2 - g, y - 0.3, y + 0.7);
    }
    // joint line of each band on its front plate, as on the original
    for (z = [[ub0, ub1], [lb0, lb1]]) xz_prism(pole_x + 2.8, pole_x + 3.2, z[0] - 1, z[1] + 1, -1, g);
}

module shell_mirrored_part() {
    difference() {
        outer();
        board_cavity();
        head_cavity();
        cover_region(0);
        // slots for the pole's two nibs, and the opening behind the boot where the cable is laid in
        z_extrude(lb0 - 1, lb0 + pole_tongue + 0.3) offset(delta = 0.15) { tongue_2d(); nib2_2d(); }
        z_extrude(lb0 - 1, head_z0 + eps) boot_pocket_2d();
        stub_dowels();
        boot_flexure_cut();
        rst_hole();
        snap_grooves();
        pry_notch();
        cosmetic_grooves();
    }
    ribs();
    boot_nub();
}

module window_cut() {
    ch = 0.4;
    hull() {
        translate([win_x0 - ch, -1, -win_h / 2 - ch]) cube([win_x1 - win_x0 + 2 * ch, 1 + eps, win_h + 2 * ch]);
        translate([win_x0, ch, -win_h / 2]) cube([win_x1 - win_x0, eps, win_h]);
    }
    translate([win_x0, 0, -win_h / 2]) cube([win_x1 - win_x0, front_t + 1, win_h]);
}

module labels_solid() {
    zc = (win_h / 2 + H / 2) / 2;
    translate([0, label_depth, 0]) rotate([90, 0, 0]) linear_extrude(label_depth) {
        translate([win_x0, zc]) text("Linie", size = label_size, font = label_font, halign = "left", valign = "center");
        translate([win_x1, zc]) text("Abfahrt in Minuten", size = label_size, font = label_font, halign = "right", valign = "center");
    }
}

module screw_marks() {
    zc = -(win_h / 2 + H / 2) / 2;
    for (x = [win_x0 + 0.9, win_x1 - 0.9])
        translate([x, -1, zc]) rotate([-90, 0, 0]) difference() {
            cylinder(d = 1.8, h = 1.3, $fn = 24);
            translate([0, 0, -1]) cylinder(d = 0.9, h = 3, $fn = 16);
        }
}

module sided() {
    if (pole_side == "left") mirror([1, 0, 0]) children();
    else children();
}

module shell() {
    difference() {
        sided() shell_mirrored_part();
        window_cut();
        labels_solid();
        screw_marks();
    }
}

// ---------------------------------------------------------------- back cover
module cover_mirrored_part() {
    difference() {
        union() {
            intersection() {   // flat back, so it prints outside-down
                cover_region(-fit);
                outer();
                xz_prism(box_l - 10, box_r + 40, -H, H, 0, D);
            }
            // pads on the standoff tips + locating pins into the M2 threads
            for (s = standoffs) {
                p = [bx + s[0], bz + (boot_bottom ? s[1] : pcb_w - s[1])];
                translate([p[0], cover_y + eps, p[1]]) rotate([90, 0, 0]) {
                    cylinder(d = 3.2, h = pad_h + eps, $fn = 24);
                    cylinder(d = 1.4, h = pad_h + 1.2, $fn = 16);
                }
            }
            // snap bumps
            yc = (cover_y + D) / 2;
            for (z = [cav_z0 + fit, cav_z1 - fit])
                translate([bx + 6, yc, z]) rotate([0, 90, 0]) cylinder(d = 0.7, h = 16, $fn = 12);
        }
        // pocket for the plug head; its corners are rounded so the round pole part keeps a wall
        z_extrude(head_z0, head_z1) offset(r = 1.2) offset(delta = -1.2)
            translate([head_x0, cover_y - 1]) square([head_x1 - head_x0, head_y0 + plug_thick + 2 * plug_clr - cover_y + 1]);
        if (header_pins) header_slits();
    }
}

// Slits for the header pins: through the cover if the pins reach the back,
// otherwise a blind pocket. A little wider than the pins for slightly offset ones.
module header_slits() {
    tip = pcb_back + header_len + 0.5;
    y1 = tip > D - 0.8 ? D + 1 : tip;
    for (v = [header_v, pcb_w - header_v])
        xz_prism(bx + header_u[0] - 1, bx + header_u[1] + 1, bz + v - header_slit_w / 2, bz + v + header_slit_w / 2, cover_y - 1, y1);
}

// The original is double-sided, so the back cover carries the same face: a black
// window with "Linie" / "Abfahrt in Minuten" above it and two screws below. It is
// the front mirrored through the box's mid-plane, so it reads from behind. The
// labels and screws sit a little closer to the window than on the front, so they
// stay on the cover instead of straddling its edge. Window and labels are an inlay
// like the front labels; the header slits pass through it.
back_mark_z = min((win_h / 2 + H / 2) / 2, cav_z1 - fit - 0.3 - label_size / 2);

module back_face_2d() {   // seen from behind: first axis = -x, second = z
    translate([-win_x1, -win_h / 2]) square([win_x1 - win_x0, win_h]);
    translate([-win_x1, back_mark_z]) text("Linie", size = label_size, font = label_font, halign = "left", valign = "center");
    translate([-win_x0, back_mark_z]) text("Abfahrt in Minuten", size = label_size, font = label_font, halign = "right", valign = "center");
}

module back_face_solid() {
    difference() {
        translate([0, D, 0]) rotate([0, 0, 180]) translate([0, label_depth, 0]) rotate([90, 0, 0])
            linear_extrude(label_depth) back_face_2d();
        if (header_pins) sided() header_slits();
    }
}

module back_screw_marks() {
    for (x = [win_x0 + 0.9, win_x1 - 0.9])
        translate([x, D + 1, -back_mark_z]) rotate([90, 0, 0]) difference() {
            cylinder(d = 1.8, h = 1.3, $fn = 24);
            translate([0, 0, -1]) cylinder(d = 0.9, h = 3, $fn = 16);
        }
}

module back_cover() {
    difference() {
        sided() cover_mirrored_part();
        back_face_solid();
        back_screw_marks();
    }
}

// ---------------------------------------------------------------- pole
module pole_mirrored_part() {
    difference() {
        union() {
            z_extrude(pole_z0, pole_top_z) pole_2d();
            z_extrude(pole_top_z - 1, pole_top_z + pole_tongue) { tongue_2d(); nib2_2d(); }   // 1 mm overlap merges cleanly
        }
        z_extrude(pole_z0 - 1, pole_top_z + 1) bore_2d();
        z_extrude(head_z0 - boot_len - 1, pole_top_z + 1) boot_pocket_2d();
    }
}

module pole() {
    sided() pole_mirrored_part();
}

// Pole above the box: plain pole, the same diameter as below, with a socket for
// the speaker plate just under the cap, as on the real stops.
spk_socket = 2.4;       // square socket in the outer side of the pole for the speaker plate
spk_socket_z = collar_top - spk_socket / 2 - 0.8;

module spk_socket_cut(clr = 0) {   // stops short of the cap's dowel hole
    s = spk_socket + 2 * clr;
    translate([pole_x + pole_r - 4, pole_y - s / 2, spk_socket_z - s / 2]) cube([10, s, s]);
}

module pole_top_mirrored_part() {
    difference() {
        translate([pole_x, pole_y, ub1]) cylinder(r = pole_r, h = collar_top - ub1, $fn = 64);
        stub_dowels();
        dowel_hole(pole_x, collar_top);
        spk_socket_cut(0.15);
    }
}

module pole_top() {
    sided() pole_top_mirrored_part();
}

// Speaker plate: a flat fan-shaped sector on the side of the band opposite the
// display, with two curved slots, as on the original. A horn hangs under each
// outer corner, one facing the front and one the back, at right angles to the
// display. Each horn is a rectangular PA horn (round driver can flaring into a
// wide rectangular mouth with a lip, bullet phase plug in the throat) held by a
// U-yoke on two pivot pins at the sides of the driver can. The plate prints
// top-down; the horns print standing on their mouths and drop into the yoke slots.
plate_t = 1.6;
plate_top = spk_socket_z + spk_socket / 2;
plate_bot = plate_top - plate_t;
drv_d = 5;              // driver can
drv_len = 3.6;
flare_len = spk_len - drv_len;
pin_d = 1.6;
plug_root_h = 3;        // phase plug: cone where it joins the throat
plug_root_d = 4.4;
yoke_gap = 0.1;         // driver can to yoke arm
yoke_t = 1.5;
yoke_fillet = 0.6;      // 45-degree fillet where each arm meets the plate
horn_z = plate_bot - drv_d / 2 - 0.8;   // pivot axis (centre of the driver can)
function plate_corner(a) = [pole_x + plate_r * cos(a), pole_y + plate_r * sin(a)];
c_front = plate_corner(spk_mount_angles[0]);
c_back = plate_corner(spk_mount_angles[1]);
// Plate size follows the yokes, so both arms of each yoke stand fully on it.
yoke_xi = drv_d / 2 + yoke_gap;                  // arm inner face from the horn axis
yoke_xo = yoke_xi + yoke_t;                       // arm outer face
yoke_hw = 1.8;                                    // arm half-width (prongs beside the pin slot: 0.9 mm)
m_a = max(abs(spk_mount_angles[0]), abs(spk_mount_angles[1]));
m_c = [plate_r * cos(m_a), plate_r * sin(m_a)];   // mount relative to the pole axis
plate_outer = norm([m_c[0] + yoke_xo + yoke_fillet, m_c[1] + yoke_hw + yoke_fillet]) + 0.5;
plate_edge = atan2(m_c[1] + yoke_hw + yoke_fillet, m_c[0] - yoke_xo - yoke_fillet) + 2;   // half-angle of the fan
slot_r = [norm([m_c[0] - yoke_xi, m_c[1] + yoke_hw]) + 0.7,     // just outside the inner arms
          norm([m_c[0] + yoke_xi, m_c[1] - yoke_hw]) - 0.7];    // just inside the outer arms

module horn_local() {   // mouth at z = 0, driver on top, pivot pins along x
    w = spk_mouth[0]; h = spk_mouth[1]; wall = 0.8;
    module mouth_2d(d = 0) { offset(r = 0.6) square([w - 1.2 - 2 * d, h - 1.2 - 2 * d], center = true); }
    difference() {
        union() {
            linear_extrude(0.8) offset(delta = 0.4) mouth_2d();                    // lip
            hull() {                                                               // flare
                linear_extrude(eps) mouth_2d();
                translate([0, 0, flare_len]) cylinder(d = drv_d, h = eps);
            }
            translate([0, 0, flare_len]) cylinder(d = drv_d, h = drv_len - 0.6);   // driver can
            translate([0, 0, spk_len - 0.6 - eps]) cylinder(d1 = drv_d, d2 = drv_d - 1.2, h = 0.6);
            translate([0, 0, flare_len + drv_len / 2]) rotate([0, 90, 0])         // pivot pins
                cylinder(d = pin_d, h = drv_d + 2 * (yoke_gap + yoke_t) - 0.2, center = true, $fn = 16);
        }
        hull() {                                                                   // hollow flare
            translate([0, 0, -eps]) linear_extrude(eps) mouth_2d(wall);
            translate([0, 0, flare_len - 0.3]) cylinder(d = drv_d - 2 * wall, h = eps);
        }
    }
    // bullet phase plug, flush with the mouth so it prints from the bed
    cylinder(d1 = 1.4, d2 = 2.4, h = 0.5);
    translate([0, 0, 0.5 - eps]) cylinder(d = 2.4, h = flare_len - 0.5);
    // The plug widens into the throat: a plain 2.4 mm column breaks off along a layer
    // line where it joins the horn.
    intersection() {
        hull() {
            linear_extrude(eps) mouth_2d();
            translate([0, 0, flare_len]) cylinder(d = drv_d, h = eps);
        }
        translate([0, 0, flare_len - plug_root_h]) cylinder(d1 = 2.4, d2 = plug_root_d, h = plug_root_h);
    }
}

module horn_placed(dir) {   // -1: front corner, faces the front; +1: back corner, faces the back
    c = dir < 0 ? c_front : c_back;
    translate([c[0], c[1], horn_z]) rotate([-dir * spk_tilt, 0, 0])
        rotate([dir * 90, 0, 0]) translate([0, 0, -(flare_len + drv_len / 2)]) horn_local();
}

// U-yoke under a plate corner: two arms beside the driver can, each with a slot
// open at the bottom so the pivot pin drops in. The arms are rounded around the
// pivot, and a fillet joins them to the plate on every face except the one
// toward the driver can (which would crowd the can and the plate slots).
module yoke(c) {
    xi = yoke_xi; hw = yoke_hw; f = yoke_fillet;
    top = plate_bot + plate_t / 2;   // arms run up into the plate so they fuse with it
    difference() {
        for (s = [-1, 1]) translate([c[0], c[1], 0]) mirror([s < 0 ? 1 : 0, 0, 0]) {
            translate([xi, 0, horn_z]) rotate([90, 0, 90]) linear_extrude(yoke_t) hull() {
                circle(hw, $fn = 32);
                translate([-hw, 0]) square([2 * hw, top - horn_z]);
            }
            hull() {
                translate([xi, -hw, plate_bot - f]) cube([yoke_t, 2 * hw, eps]);
                translate([xi + f, -hw - f, plate_bot]) cube([yoke_t, 2 * (hw + f), plate_t / 2]);
            }
        }
        translate([c[0], c[1], horn_z]) rotate([0, 90, 0]) cylinder(d = pin_d + 0.2, h = 20, center = true, $fn = 16);
        translate([c[0] - 10, c[1] - (pin_d + 0.2) / 2, horn_z - 3]) cube([20, pin_d + 0.2, 3]);
        translate([pole_x, pole_y, horn_z - 5]) cylinder(r = pole_r + 0.2, h = 20, $fn = 64);   // fillets stop at the pole
    }
}

module plate_2d() {
    a0 = -plate_edge; a1 = plate_edge;
    difference() {
        intersection() {
            translate([pole_x, pole_y]) circle(plate_outer, $fn = 96);
            translate([pole_x, pole_y]) polygon([[0, 0], 40 * [cos(a0), sin(a0)], 40 * [cos(a1), sin(a1)]]);
        }
        translate([pole_x, pole_y]) circle(pole_r + 0.1, $fn = 64);   // sits against the pole
        // the two curved slots, between the inner and outer yoke arms
        for (r = slot_r) translate([pole_x, pole_y]) intersection() {
            difference() { circle(r + 0.4, $fn = 96); circle(r - 0.4, $fn = 96); }
            polygon([[0, 0], 40 * [cos(a0 + 12), sin(a0 + 12)], 40 * [cos(a1 - 12), sin(a1 - 12)]]);
        }
    }
}

module speaker_bracket_mirrored_part() {
    p = spk_socket;
    x0 = pole_x + pole_r - 3.5;       // peg end, inside the pole socket
    translate([x0, pole_y - p / 2, plate_top - p]) cube([5, p, p]);   // peg
    z_extrude(plate_bot, plate_top) plate_2d();
    yoke(c_front);
    yoke(c_back);
}

module speaker_bracket() {
    sided() speaker_bracket_mirrored_part();
}

module horns() {
    sided() { horn_placed(1); horn_placed(-1); }
}

// Red cap: a plain half ball the diameter of the pole, as on the real stops.
module cap_mirrored_part() {
    difference() {
        translate([pole_x, pole_y, collar_top]) rotate_extrude($fn = 96) intersection() {
            circle(pole_r, $fn = 96);
            square(pole_r);
        }
        dowel_hole(pole_x, collar_top);
    }
}

module cap() {
    sided() cap_mirrored_part();
}

// Cover for the pole's rear slot. Its outer face is the arc that the pole's back
// flat cuts off, so the assembled pole is round. A tongue fills the slot, and the
// lip on its tip snaps behind the slot edges (the slotted pole opens a little to
// let it through). At the top it also closes the opening the plug's boot goes in
// through, leaving room for the boot. Prints arc-down.
module pole_cover_mirrored_part() {
    br = pole_bore / 2;
    tw = slot_w / 2 - 0.025;                                 // tongue half-width: a snug fit
    lip_v = sqrt(br * br - pow(tw + cover_lip, 2)) - 0.05;   // lip shoulder, just inside the bore
    boot_z0 = head_z0 - boot_len - 1;                        // bottom of the boot opening
    z0 = base_top + 0.1;
    z1 = pole_top_z - 0.1;
    bu = boot_x - pole_x;
    bv = usb_y - pole_y;
    translate([pole_x, pole_y, 0]) {
        z_extrude(z0, z1) intersection() {   // 0.2 mm flat on the crest: ~3 mm of bed contact
            circle(pole_r, $fn = 64);
            translate([-pole_r, flat_y]) square([2 * pole_r, pole_r - 0.2 - flat_y]);
        }
        z_extrude(z0, boot_z0) {
            translate([-tw, lip_v]) square([2 * tw, flat_y - lip_v + eps]);
            polygon([[-tw - cover_lip, lip_v], [tw + cover_lip, lip_v], [tw - 0.3, lip_v - 0.8], [-tw + 0.3, lip_v - 0.8]]);
        }
        // fills the boot opening around the back of the boot
        z_extrude(boot_z0, z1) difference() {
            intersection() {
                circle(pole_r, $fn = 64);
                translate([-pole_r - 1, bv]) square([bu + boot_d / 2 + pole_r + 1, pole_r - 0.2 - bv]);
            }
            translate([bu, bv]) circle(d = boot_d + 0.4);
        }
    }
}

module pole_cover() {
    sided() pole_cover_mirrored_part();
}

// ---------------------------------------------------------------- base (platform + track)
rail_x = plat_x0 - rail_off;          // field-side edge of the near rail's head

module base_body() {
    x0 = base_x0; x1 = base_x1;
    y0 = base_y0; y1 = base_y1;
    z0 = base_top - base_h;
    ch = 0.8;
    // platform; the chamfer along the track side is the rounded top of the edge stones
    hull() {
        translate([plat_x0, y0, z0]) cube([x1 - plat_x0, y1 - y0, base_h - ch]);
        translate([plat_x0 + ch, y0 + ch, z0]) cube([x1 - plat_x0 - 2 * ch, y1 - y0 - 2 * ch, base_h]);
    }
    // track strip
    hull() {
        translate([x0, y0, z0]) cube([plat_x0 - x0 + ch, y1 - y0, base_h - curb_drop - ch]);
        translate([x0 + ch, y0 + ch, z0]) cube([plat_x0 - x0, y1 - y0 - 2 * ch, base_h - curb_drop]);
    }
}

module base_joints() {
    y0 = base_y0; y1 = base_y1;
    j = joint_w / 2;
    // platform: joints between the edge stones, and along the asphalt
    z_extrude(base_top - joint_d, base_top + 1) {
        for (y = [y0 + block_l : block_l : y1 - 1]) translate([plat_x0 - 1, y - j]) square([edge_w + 1, joint_w]);
        translate([plat_x0 + edge_w - j, y0 - 1]) square([joint_w, y1 - y0 + 2]);
    }
    // track: the strip along the curb, joints along both sides of the rail
    // (head 1.6, groove 1.0, guard lip 0.6) and between the slabs, and the rail's groove
    z_extrude(street_top - joint_d, street_top + 1) {
        for (x = [plat_x0 - 1.2, rail_x, rail_x - 3.2]) translate([x - j, y0 - 1]) square([joint_w, y1 - y0 + 2]);
        for (y = [(y0 + y1) / 2]) {           // one joint across the track, mid-way
            translate([rail_x, y - j]) square([plat_x0 - 1.2 - rail_x, joint_w]);
            translate([base_x0 - 1, y - j]) square([rail_x - 3.2 - base_x0 + 1, joint_w]);
        }
    }
    z_extrude(street_top - 1.2, street_top + 1) translate([rail_x - 2.6, y0 - 1]) square([1, y1 - y0 + 2]);
}

module base_cuts() {
    x0 = base_x0; x1 = base_x1;
    y0 = base_y0; y1 = base_y1;
    z0 = base_top - base_h;
    // pole socket
    z_extrude(base_top - socket_depth, base_top + 1) offset(delta = 0.15) pole_2d();
    // Slit through the full height from the pole's rear slot to the back edge.
    // Both cable ends carry plugs, so the cable is laid in from above rather than
    // threaded; it rests on the bottom and the base strip closes the top.
    translate([pole_x - base_slit / 2, pole_y, z0 - 1]) cube([base_slit, y1 - pole_y + 1, base_h + 2]);
    // feet recesses, clear of the slit
    for (p = [[x0 + 7, y0 + 7], [x1 - 7, y0 + 7], [x0 + 7, y1 - 7], [x1 - 7, y1 - 7]])
        translate([p[0], p[1], z0 - 1]) cylinder(d = 8.4, h = 1.6);
    // sockets for the railing's end posts
    for (y = fence_y) translate([fence_x, y, base_top - 3.2]) cylinder(d = fence_d + 0.3, h = 5, $fn = 24);
    base_joints();
}

module base_mirrored_part() {
    difference() {
        base_body();
        base_cuts();
    }
}

module base() {
    sided() base_mirrored_part();
}

// Strip that closes the slit above the cable, flush with the pavement. Its
// underside ramps down from the pole, so the cable can bend from the pole's rear
// slot to the bottom of the slit. Press-fit; prints on its side.
module base_strip_mirrored_part() {
    w = base_slit - 0.1;
    ys = pole_y + flat_y + 0.25;                       // just behind the pole socket
    zc = base_top - base_h + cable_d + 0.6;            // top of the cable channel
    translate([pole_x - w / 2, 0, 0]) rotate([90, 0, 90]) linear_extrude(w)
        polygon([[ys, base_top], [base_y1, base_top], [base_y1, zc], [ys + 8, zc], [ys, base_top - 2.5]]);
}

module base_strip() {
    sided() base_strip_mirrored_part();
}

// ---------------------------------------------------------------- railing and Mistkuebel
// One tube bent into an inverted U, plus a lower rail. The posts plug 3 mm into
// the base. Two ways to print it: in one piece, lying on its roadway side, which
// is flattened a little for that (the underside comes out less round); or split
// lengthwise into two identical halves, each printed on its cut face and pinned
// back together with three pieces of 1.75 mm filament.
module fence_tubes() {
    r = fence_d / 2;
    R = fence_r;
    ya = fence_y[0]; yb = fence_y[1];
    zt = base_top + fence_h;
    zb = base_top - 3;
    module bend() {   // quarter bend in the yz plane, from +y round to +z
        multmatrix([[0, 0, 1, 0], [1, 0, 0, 0], [0, 1, 0, 0]])
            rotate_extrude(angle = 90, $fn = 48) translate([R, 0]) circle(r, $fn = 24);
    }
    translate([fence_x, 0, 0]) {
        translate([0, ya + R - eps, zt]) rotate([-90, 0, 0]) cylinder(r = r, h = yb - ya - 2 * R + 2 * eps, $fn = 24);
        translate([0, ya + R, zt - R]) mirror([0, 1, 0]) bend();
        translate([0, yb - R, zt - R]) bend();
        for (y = [ya, yb]) translate([0, y, zb]) cylinder(r = r, h = zt - R - zb + eps, $fn = 24);
        translate([0, ya, base_top + fence_mid]) rotate([-90, 0, 0]) cylinder(r = r, h = yb - ya, $fn = 24);
    }
}

module fence_mirrored_part() {
    intersection() {
        fence_tubes();
        translate([fence_x - 5, fence_y[0] - 10, base_top - 4]) cube([5 + fence_d / 2 - 0.3, fence_y[1] - fence_y[0] + 20, fence_h + 10]);
    }
}

// The roadway half. The pin holes sit symmetric about the railing's middle, so
// the other half is the same part turned round.
module fence_half_mirrored_part() {
    difference() {
        intersection() {
            fence_tubes();
            translate([fence_x, fence_y[0] - 10, base_top - 4]) cube([5, fence_y[1] - fence_y[0] + 20, fence_h + 10]);
        }
        for (p = [[fence_y[0], fence_mid], [fence_y[1], fence_mid], [(fence_y[0] + fence_y[1]) / 2, fence_h]])
            translate([fence_x - 1, p[0], base_top + p[1]]) rotate([0, 90, 0]) cylinder(d = 1.85, h = 2, $fn = 16);
    }
}

module fence() {
    sided() fence_mirrored_part();
}

module fence_half() {
    sided() fence_half_mirrored_part();
}

// Vienna's street litter bin: an oval body, slightly narrower at the bottom, a
// domed lid, the oval opening (with its rim) on top facing forwards, a bottom
// flange, and the ash tube for cigarettes rising out of the rear of the lid. Back at x = 0
// (against the railing), front towards -x, bottom at z = 0. Grooves in the back
// take the top and lower rail.
can_dome = 12;
can_taper = 0.93;       // body width at the bottom, relative to the top
can_flange = 1.5;

function can_scale(z) = can_taper + (1 - can_taper) * min(1, max(0, (z - can_flange) / (can_size[2] - can_dome - can_flange)));

// Footprint: a superellipse, between an oval and a rounded rectangle, so the
// outline curves everywhere but the sides stay full.
can_round = 3;          // 2 = ellipse; larger = squarer
can_lid_round = 2.2;    // lid profile, the same way

module can_plan(g = 0, s = 1) {
    w = can_size[0]; d = can_size[1];
    a = s * d / 2 + g; b = s * w / 2 + g; e = 2 / can_round;
    function sp(v) = sign(v) * pow(abs(v), e);
    translate([-d / 2, 0]) polygon([for (t = [0 : 4 : 359]) [a * sp(cos(t)), b * sp(sin(t))]]);
}

module can_shape(g = 0) {
    h = can_size[2]; dh = can_dome; f = can_flange;
    linear_extrude(f) offset(r = 0.6) can_plan(g, can_taper);   // bottom flange
    hull() {                                                   // body
        translate([0, 0, f - eps]) linear_extrude(eps) can_plan(g, can_taper);
        translate([0, 0, h - dh]) linear_extrude(eps) can_plan(g);
    }
    hull() for (i = [0 : 12]) {                                // lid: domed right to the top, no flat
        t = i / 12;
        translate([0, 0, h - dh + dh * t])
            linear_extrude(eps) can_plan(g, max(0.12, pow(1 - pow(t, can_lid_round), 1 / can_lid_round)));
    }
}

module can_local() {
    w = can_size[0]; d = can_size[1]; h = can_size[2];
    tube = [-6, -8.5];                // ash tube: out of the lid, right side seen from the front, inside the body outline
    tube_top = h + 4;
    floor_z = 10;                     // the hollow inside reaches down to here
    module at_opening() {             // on the front of the lid, where its surface faces 45 degrees forwards
        translate([-d / 2 - 0.55 * d / 2, 0, h - 0.2 * can_dome]) rotate([0, -45, 0]) children();
    }
    module opening_2d(dd) { scale([8 / 11, 1]) circle(d = dd, $fn = 48); }
    module opening(dd) { at_opening() translate([0, 0, -9]) linear_extrude(14) opening_2d(dd); }
    // Inside: ~1.3 mm walls, kept clear of the top-rail groove at the back and of
    // the ash tube, which stands in it as a column. The roof under the lid slopes
    // steeply enough to print without support.
    module inside_2d(z) {
        intersection() {
            can_plan(-1.3, can_scale(z));
            translate([-50, -50]) square([47, 100]);
        }
    }
    module inside() {
        difference() {
            hull() {
                translate([0, 0, floor_z]) linear_extrude(eps) inside_2d(floor_z);
                translate([0, 0, h - can_dome]) linear_extrude(eps) inside_2d(h - can_dome);
                at_opening() translate([0, 0, -1.7]) linear_extrude(eps) opening_2d(10.6);
            }
            translate([tube[0], tube[1], 0]) cylinder(r = 2.6, h = h + 1, $fn = 24);
        }
    }
    difference() {
        union() {
            can_shape();
            intersection() {                                   // rim round the opening
                can_shape(0.35);
                difference() { opening(11 + 1.6); opening(11); }
            }
            translate([tube[0], tube[1], floor_z]) cylinder(d = 3.2, h = tube_top - floor_z, $fn = 24);
        }
        opening(11);
        inside();
        translate([tube[0], tube[1], tube_top - 1.5]) cylinder(d = 1.4, h = 3, $fn = 16);   // where the ash goes
        translate([tube[0], tube[1], h + 0.3]) difference() {                               // edge of its orange tip
            cylinder(d = 4, h = 0.3, $fn = 24);
            translate([0, 0, -1]) cylinder(d = 2.6, h = 3, $fn = 24);
        }
        for (z = [can_band[0], can_band[1], h - can_dome])   // band edges and the lid seam
            translate([0, 0, z - 0.15]) linear_extrude(0.3) difference() {
                offset(r = 2) can_plan(0, can_scale(z));
                offset(delta = -0.3) can_plan(0, can_scale(z));
            }
        for (z = [fence_h, fence_mid])                        // grooves for the top and lower rail
            translate([fence_d / 2 - 1.1, -w, z - can_z0]) rotate([-90, 0, 0])
                cylinder(d = fence_d + 0.2, h = 2 * w, $fn = 24);
    }
}

module can() {
    sided() translate([fence_x - fence_d / 2 + 1.1, can_y, base_top + can_z0]) can_local();
}

// ---------------------------------------------------------------- screen dummy (static model)
// Stand-in for the board, for a model without electronics: the outline of the LCD
// module and the PCB, plus the four standoffs the back cover's pins locate in. It
// sits where the board would and the cover presses it forward the same way. Printed
// in black, its front reads as a switched-off display. The PCB part is 0.1 mm smaller
// all round for print tolerance; the LCD part keeps its size so it fills the window.
module screen_dummy() {
    sided() difference() {
        union() {
            translate([bx + (pcb_l - 36) / 2, front_t, bz + (pcb_w - 19.6) / 2]) cube([36, glass_to_pcb + eps, 19.6]);
            translate([bx + 0.1, pcb_front, bz + 0.1]) cube([pcb_l - 0.2, pcb_t, pcb_w - 0.2]);
            for (s = standoffs)
                translate([bx + s[0], pcb_back - eps, bz + s[1]]) rotate([-90, 0, 0]) cylinder(d = 3.5, h = standoff_h + eps, $fn = 24);
        }
        for (s = standoffs)   // for the cover's locating pins
            translate([bx + s[0], pcb_back, bz + s[1]]) rotate([-90, 0, 0]) cylinder(d = 1.8, h = standoff_h + 1, $fn = 16);
    }
}

// ---------------------------------------------------------------- dummy board (preview only)
module board_dummy() {
    sided() {
        color("black") translate([bx + (pcb_l - 36) / 2, front_t, bz + (pcb_w - 19.6) / 2]) cube([36, glass_to_pcb, 19.6]);
        color("darkgreen") translate([bx, pcb_front, bz]) cube([pcb_l, pcb_t, pcb_w]);
        color("silver") translate([x_rf - 7.35, pcb_back, usb_z - 4.47]) cube([7.35, usb_h, 8.94]);
        color("gold") for (s = standoffs)
            translate([bx + s[0], pcb_back, bz + s[1]]) rotate([-90, 0, 0]) cylinder(d = 3.5, h = standoff_h);
        if (header_pins) for (v = [header_v, pcb_w - header_v]) {
            n = round((header_u[1] - header_u[0]) / 2.54);
            color("black") xz_prism(bx + header_u[0] - 1.27, bx + header_u[1] + 1.27, bz + v - 1.27, bz + v + 1.27,
                                    pcb_back, pcb_back + header_body);
            color("gold") for (i = [0 : n]) let (u = header_u[0] + i * 2.54)
                xz_prism(bx + u - 0.32, bx + u + 0.32, bz + v - 0.32, bz + v + 0.32, pcb_back, pcb_back + header_len);
        }
        color("dimgray", 0.6) xz_prism(x_rf, x_rf + plug_depth, usb_z - plug_down, usb_z + plug_up,
                                       usb_y - plug_thick / 2, usb_y + plug_thick / 2);
    }
    // lit area where the fit test found it, centred in the shifted window
    color([1, 0.6, 0]) translate([(win_x0 + win_x1) / 2 - aa_w / 2, front_t - 0.05, -aa_h / 2]) cube([aa_w, 0.05, aa_h]);
}

// ---------------------------------------------------------------- output
// Print orientation: shell and labels face-down, cover outside-down, pole on its
// back flat, pole cover arc-down, base strip on its side, speaker plate top-down,
// horns on their mouths. Slicers drop parts to the bed.
module print_face_down() { rotate([90, 0, 0]) children(); }
module print_back_down() { translate([0, 0, D]) rotate([-90, 0, 0]) children(); }

if (part == "assembly") {
    color("lightgray") shell();
    color("black") translate([0, -0.02, 0]) labels_solid();   // proud by a hair so the preview shows it
    color("gray") back_cover();
    color("black") translate([0, 0.02, 0]) back_face_solid();   // proud by a hair so the preview shows it
    color("silver") pole();
    color("silver") pole_top();
    color("gray") speaker_bracket();
    color("gray") horns();
    color("red") cap();
    color("silver") pole_cover();
    color("gainsboro") base();
    color("gainsboro") base_strip();
    color("silver") fence();
    color("lightgray") can();
    board_dummy();
} else if (part == "shell") {
    print_face_down() shell();
} else if (part == "labels") {
    print_face_down() labels_solid();
} else if (part == "screen_dummy") {
    print_face_down() screen_dummy();         // display face down
} else if (part == "back") {
    print_back_down() back_cover();
} else if (part == "back_labels") {
    print_back_down() back_face_solid();
} else if (part == "pole") {
    print_back_down() pole();
} else if (part == "pole_top") {
    translate([0, 0, -ub1]) pole_top();
} else if (part == "speaker_bracket") {
    rotate([180, 0, 0]) speaker_bracket();  // plate top on the bed, hangers up
} else if (part == "horn") {
    horn_local();                           // print two, standing on the mouth lip
} else if (part == "cap") {
    translate([0, 0, -collar_top]) cap();   // flat side down, dome up
} else if (part == "base") {
    translate([0, 0, -(base_top - base_h)]) base();
} else if (part == "pole_cover") {
    rotate([-90, 0, 0]) pole_cover();       // arc on the bed, tongue up
} else if (part == "fence") {
    rotate([0, 90, 0]) fence();             // lying on its flattened roadway side
} else if (part == "fence_half") {
    rotate([0, -90, 0]) fence_half();       // cut face down; print two
} else if (part == "can") {
    can_local();                            // upright on its flange
} else if (part == "base_strip") {
    rotate([0, -90, 0]) base_strip();       // on its side
} else if (part == "fittest") {
    print_face_down() intersection() {
        shell();
        // bezel, ribs and the start of the BOOT flexure; stops short of the pole
        translate([box_l - 1, -1, -H / 2 - 1]) cube([W + 1.6, 1 + 6.3, H + 2]);
    }
} else if (part == "cabletest") {
    // Cable fit test: the pole end of the box with the plug-head pocket, the brackets
    // and the opening for the boot, plus the whole BOOT flexure. Put the board in with
    // the cable plugged in.
    print_face_down() intersection() {
        shell();
        translate([btn_x - 10, -1, -H / 2 - 1]) cube([60, D + 2, H + 2]);   // from just before the BOOT flexure
    }
} else if (part == "cabletest_pole") {
    // the top of the pole, nibs included, where the boot drops in
    print_back_down() intersection() {
        pole();
        translate([pole_x - 20, pole_y - 20, pole_top_z - 16]) cube([40, 40, 16 + pole_tongue + 1]);
    }
} else if (part == "cabletest_cover") {
    // the top of the pole cover, which closes round the boot
    rotate([-90, 0, 0]) intersection() {
        pole_cover();
        translate([pole_x - 20, pole_y - 20, pole_top_z - 16]) cube([40, 40, 17]);
    }
}
