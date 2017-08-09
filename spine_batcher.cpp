/*************************************************************************/
/*  spine_batcher.cpp                                                    */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                    http://www.godotengine.org                         */
/*************************************************************************/
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                 */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/
#ifdef MODULE_SPINE_ENABLED
#include "spine_batcher.h"

#define BATCH_CAPACITY 1024
#define BATCH_ELEMENTS_POOL 64



List<SpineBatcher::Elements::Command *> SpineBatcher::Elements::free;

SpineBatcher::SetBlendMode::SetBlendMode(int p_mode) {

	cmd = CMD_SET_BLEND_MODE;
	mode = p_mode;
}

void SpineBatcher::SetBlendMode::draw(RID ci) {

	VisualServer::get_singleton()->canvas_item_add_set_blend_mode(ci, VS::MaterialBlendMode(mode));
}

SpineBatcher::Elements::Elements() {

	cmd = CMD_DRAW_ELEMENT;
	vertices = memnew_arr(Vector2, BATCH_CAPACITY);
	colors = memnew_arr(Color, BATCH_CAPACITY);
	uvs = memnew_arr(Vector2, BATCH_CAPACITY);
	indies = memnew_arr(int, BATCH_CAPACITY * 3);
	vertices_count = 0;
	indies_count = 0;
};

SpineBatcher::Elements::~Elements() {

	memdelete_arr(vertices);
	memdelete_arr(colors);
	memdelete_arr(uvs);
	memdelete_arr(indies);
}



void SpineBatcher::Elements::draw(RID ci) {

	VisualServer::get_singleton()->canvas_item_add_triangle_array_ptr(ci,
		indies_count / 3,
		indies,
		vertices,
		colors,
		uvs,
		texture->get_rid()
	);
}

SpineBatcher::Command* SpineBatcher::Elements::take(){
    if (free.size() == 0){
        print_line("preallocate " + itos(BATCH_ELEMENTS_POOL) + " SpineBatcher::Elements");
        for (int i=0; i<BATCH_ELEMENTS_POOL; i++){
            free.push_back(memnew(Elements));
        }
    }
    Command* f = free[0];
    free.pop_front();
    return f;
}

void SpineBatcher::Elements::retrieve(Command* elem){
    Elements* e = reinterpret_cast<Elements*>(elem);
    if (e){
        e->vertices_count = 0;
        e->indies_count = 0;
        free.push_back(e);
    } else {
        memdelete(elem);
    }
}

void SpineBatcher::add(Ref<Texture> p_texture,
	const float* p_vertices, const float* p_uvs, int p_vertices_count,
	const unsigned short* p_indies, int p_indies_count,
	Color *p_color, bool flip_x, bool flip_y) {

    //print_line("add vertices" + itos(p_vertices_count) + ":" + itos(elements->vertices_count) + "  " + 
    //                           itos(p_indies_count) + ":" + itos(elements->indies_count));

    if ( (p_vertices_count >> 1) >= BATCH_CAPACITY || p_indies_count >= BATCH_CAPACITY * 3) return;

    //print_line("adding");
	if (p_texture != elements->texture
		|| elements->vertices_count + (p_vertices_count >> 1) >= BATCH_CAPACITY
		|| elements->indies_count + p_indies_count >= BATCH_CAPACITY * 3) {
	
        //print_line("add oversized, push_elements");
		push_elements();
		elements->texture = p_texture;
	}

	for (int i = 0; i < p_indies_count; ++i, ++elements->indies_count)
		elements->indies[elements->indies_count] = p_indies[i] + elements->vertices_count;
    //print_line("add indicess added");
	for (int i = 0; i+1 < p_vertices_count; i += 2, ++elements->vertices_count) {

		elements->vertices[elements->vertices_count].x = flip_x ? -p_vertices[i] : p_vertices[i];
		elements->vertices[elements->vertices_count].y = flip_y ? p_vertices[i + 1] : -p_vertices[i + 1];
		elements->colors[elements->vertices_count] = *p_color;
		elements->uvs[elements->vertices_count].x = p_uvs[i];
		elements->uvs[elements->vertices_count].y = p_uvs[i + 1];
	}
}

void SpineBatcher::add_set_blender_mode(bool p_mode) {

	push_elements();
	element_list.push_back(memnew(SetBlendMode(p_mode)));
}

void SpineBatcher::flush() {

	RID ci = owner->get_canvas_item();
	push_elements();

	for (List<Command *>::Element *E = element_list.front(); E; E = E->next()) {

		Command *e = E->get();
		e->draw(ci);
		drawed_list.push_back(e);
	}
	element_list.clear();
}

void SpineBatcher::push_elements() {

	if (elements->vertices_count <= 0 || elements->indies_count <= 0)
		return;

	element_list.push_back(elements);
	elements = reinterpret_cast<Elements *>(Elements::take());
}

void SpineBatcher::reset() {

	for (List<Command *>::Element *E = drawed_list.front(); E; E = E->next()) {

		Command *e = E->get();
        Elements::retrieve(e);
	}
	drawed_list.clear();
}

SpineBatcher::SpineBatcher(Node2D *owner) : owner(owner) {

	elements = reinterpret_cast<Elements *>(Elements::take());
}

SpineBatcher::~SpineBatcher() {

	for (List<Command *>::Element *E = element_list.front(); E; E = E->next()) {

		Command *e = E->get();
        Elements::retrieve(e);
	}
	element_list.clear();

	for (List<Command *>::Element *E = drawed_list.front(); E; E = E->next()) {

		Command *e = E->get();
        Elements::retrieve(e);
	}
	drawed_list.clear();

    Elements::retrieve(elements);
}

#endif // MODULE_SPINE_ENABLED

