// Exports the parts in their assembled positions for render.py, one per run:
//   openscad -D 'part="none"' -D 'what="shell"' -o build/stl/shell.stl export.scad
// The closed back (static model) is "back"/"back_labels" with -D header_pins=false.
// Every run also echoes RENDER = [...], the few dimensions render.py needs.
include <../wl_display.scad>

what = "";

module can_at() { translate([fence_x - fence_d / 2 + 1.1, can_y, base_top + can_z0]) children(); }

// The bin's orange: the band, the tip of the ash tube, and the rim of the opening
// (placed like the opening in can_local()).
module orange_zones() {
    h = can_size[2]; d = can_size[1];
    translate([-500, -500, base_top + can_z0 + can_band[0]]) cube([1000, 1000, can_band[1] - can_band[0]]);
    translate([-500, -500, base_top + can_z0 + h + 0.3]) cube([1000, 1000, 20]);
    can_at() translate([-d / 2 - 0.55 * d / 2, 0, h - 0.2 * can_dome]) rotate([0, -45, 0]) translate([0, 0, -1.2])
        linear_extrude(6) scale([8 / 11, 1]) circle(d = 13, $fn = 48);
}

if (what == "shell") shell();
if (what == "labels") labels_solid();
if (what == "back") back_cover();
if (what == "back_labels") back_face_solid();
if (what == "pole") pole();
if (what == "pole_top") pole_top();
if (what == "pole_cover") pole_cover();
if (what == "speaker_bracket") speaker_bracket();
if (what == "horns") horns();
if (what == "cap") cap();
if (what == "base") base();
if (what == "base_strip") base_strip();
if (what == "fence") fence();
if (what == "can_grey") difference() { can(); orange_zones(); }
if (what == "can_orange") intersection() { can(); orange_zones(); }
if (what == "screen_dummy") screen_dummy();

// the board, split by material (same geometry as board_dummy())
if (what == "board_lcd") sided() translate([bx + (pcb_l - 36) / 2, front_t, bz + (pcb_w - 19.6) / 2]) cube([36, glass_to_pcb, 19.6]);
if (what == "board_pcb") sided() translate([bx, pcb_front, bz]) cube([pcb_l, pcb_t, pcb_w]);
if (what == "board_metal") sided() {
    translate([x_rf - 7.35, pcb_back, usb_z - 4.47]) cube([7.35, usb_h, 8.94]);
    for (s = standoffs) translate([bx + s[0], pcb_back, bz + s[1]]) rotate([-90, 0, 0]) cylinder(d = 3.5, h = standoff_h);
    for (v = [header_v, pcb_w - header_v]) {
        n = round((header_u[1] - header_u[0]) / 2.54);
        for (i = [0 : n]) let (u = header_u[0] + i * 2.54)
            xz_prism(bx + u - 0.32, bx + u + 0.32, bz + v - 0.32, bz + v + 0.32, pcb_back, pcb_back + header_len);
    }
}
if (what == "board_black") sided() {
    for (v = [header_v, pcb_w - header_v])
        xz_prism(bx + header_u[0] - 1.27, bx + header_u[1] + 1.27, bz + v - 1.27, bz + v + 1.27, pcb_back, pcb_back + header_body);
    xz_prism(x_rf, x_rf + plug_depth, usb_z - plug_down, usb_z + plug_up, usb_y - plug_thick / 2, usb_y + plug_thick / 2);
}

// base bottom, window centre x, active area w/h, glass front (all mm)
echo(RENDER = [base_top - base_h, (win_x0 + win_x1) / 2, aa_w, aa_h, front_t]);
