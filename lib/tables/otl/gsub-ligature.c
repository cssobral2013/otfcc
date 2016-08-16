#include "gsub-ligature.h"
static void deleteGSUBLigatureSubtable(otl_subtable *_subtable) {
	if (!_subtable) return;
	subtable_gsub_ligature *subtable = &(_subtable->gsub_ligature);
	if (subtable->from && subtable->to) {
		for (uint16_t j = 0; j < subtable->to->numGlyphs; j++) {
			caryll_delete_coverage(subtable->from[j]);
		}
		free(subtable->from);
	}
	caryll_delete_coverage(subtable->to);
	free(subtable);
}
void caryll_delete_gsub_ligature(otl_lookup *lookup) {
	if (lookup) {
		if (lookup->subtables) {
			for (uint16_t j = 0; j < lookup->subtableCount; j++)
				if (lookup->subtables[j]) { deleteGSUBLigatureSubtable(lookup->subtables[j]); }
			free(lookup->subtables);
		}
		FREE(lookup);
	}
}
otl_subtable *caryll_read_gsub_ligature(font_file_pointer data, uint32_t tableLength, uint32_t offset) {
	otl_subtable *_subtable;
	NEW(_subtable);
	subtable_gsub_ligature *subtable = &(_subtable->gsub_ligature);
	subtable->from = NULL;
	subtable->to = NULL;
	checkLength(offset + 6);

	otl_coverage *startCoverage = caryll_read_coverage(data, tableLength, offset + read_16u(data + offset + 2));
	if (!startCoverage) goto FAIL;
	uint16_t setCount = read_16u(data + offset + 4);
	if (setCount != startCoverage->numGlyphs) goto FAIL;
	checkLength(offset + 6 + setCount * 2);

	uint32_t ligatureCount = 0;
	for (uint16_t j = 0; j < setCount; j++) {
		uint32_t setOffset = offset + read_16u(data + offset + 6 + j * 2);
		checkLength(setOffset + 2);
		ligatureCount += read_16u(data + setOffset);
		checkLength(setOffset + 2 + read_16u(data + setOffset) * 2);
	}

	NEW(subtable->to);
	subtable->to->numGlyphs = ligatureCount;
	NEW_N(subtable->to->glyphs, ligatureCount);
	NEW_N(subtable->from, ligatureCount);
	for (uint16_t j = 0; j < ligatureCount; j++) {
		subtable->from[j] = NULL;
	};
	uint16_t jj = 0;
	for (uint16_t j = 0; j < setCount; j++) {
		uint32_t setOffset = offset + read_16u(data + offset + 6 + j * 2);
		uint16_t lc = read_16u(data + setOffset);
		for (uint16_t k = 0; k < lc; k++) {
			uint32_t ligOffset = setOffset + read_16u(data + setOffset + 2 + k * 2);
			checkLength(ligOffset + 4);
			uint16_t ligComponents = read_16u(data + ligOffset + 2);
			checkLength(ligOffset + 2 + ligComponents * 2);

			subtable->to->glyphs[jj] = handle_from_id(read_16u(data + ligOffset));

			NEW(subtable->from[jj]);
			subtable->from[jj]->numGlyphs = ligComponents;
			NEW_N(subtable->from[jj]->glyphs, ligComponents);
			subtable->from[jj]->glyphs[0] = handle_from_id(startCoverage->glyphs[j].index);
			for (uint16_t m = 1; m < ligComponents; m++) {
				subtable->from[jj]->glyphs[m] = handle_from_id(read_16u(data + ligOffset + 2 + m * 2));
			}
			jj++;
		}
	}
	caryll_delete_coverage(startCoverage);
	return _subtable;
FAIL:
	deleteGSUBLigatureSubtable(_subtable);
	return NULL;
}

json_value *caryll_gsub_ligature_to_json(otl_subtable *_subtable) {
	subtable_gsub_ligature *subtable = &(_subtable->gsub_ligature);
	json_value *st = json_array_new(subtable->to->numGlyphs);
	for (uint16_t j = 0; j < subtable->to->numGlyphs; j++) {
		json_value *entry = json_object_new(2);
		json_object_push(entry, "from", caryll_coverage_to_json(subtable->from[j]));
		json_object_push(entry, "to", json_string_new_length((uint32_t)sdslen(subtable->to->glyphs[j].name),
		                                                     subtable->to->glyphs[j].name));
		json_array_push(st, preserialize(entry));
	}
	json_value *ret = json_object_new(1);
	json_object_push(ret, "substitutions", st);
	return ret;
}

otl_subtable *caryll_gsub_ligature_from_json(json_value *_subtable) {
	otl_subtable *_st;
	if (json_obj_get_type(_subtable, "substitutions", json_array)) {
		_subtable = json_obj_get_type(_subtable, "substitutions", json_array);

		NEW(_st);
		subtable_gsub_ligature *st = &(_st->gsub_ligature);
		NEW(st->to);
		st->to->numGlyphs = _subtable->u.array.length;
		NEW_N(st->to->glyphs, st->to->numGlyphs);
		NEW_N(st->from, st->to->numGlyphs);

		uint16_t jj = 0;
		for (uint16_t k = 0; k < st->to->numGlyphs; k++) {
			json_value *entry = _subtable->u.array.values[k];
			json_value *_from = json_obj_get_type(entry, "from", json_array);
			json_value *_to = json_obj_get_type(entry, "to", json_string);
			if (!_from || !_to) continue;
			st->to->glyphs[jj].name = sdsnewlen(_to->u.string.ptr, _to->u.string.length);
			st->from[jj] = caryll_coverage_from_json(_from);
			jj += 1;
		}
		st->to->numGlyphs = jj;

	} else {
		NEW(_st);
		subtable_gsub_ligature *st = &(_st->gsub_ligature);
		NEW(st->to);
		st->to->numGlyphs = _subtable->u.object.length;
		NEW_N(st->to->glyphs, st->to->numGlyphs);
		NEW_N(st->from, st->to->numGlyphs);

		uint16_t jj = 0;
		for (uint16_t k = 0; k < st->to->numGlyphs; k++) {
			json_value *_from = _subtable->u.object.values[k].value;
			if (!_from || _from->type != json_array) continue;
			st->to->glyphs[jj].name =
			    sdsnewlen(_subtable->u.object.values[k].name, _subtable->u.object.values[k].name_length);
			st->from[jj] = caryll_coverage_from_json(_from);
			jj += 1;
		}
		st->to->numGlyphs = jj;
	}
	return _st;
}

typedef struct {
	int gid;
	int ligid;
	UT_hash_handle hh;
} ligature_aggerator;
static int by_gid(ligature_aggerator *a, ligature_aggerator *b) {
	return a->gid - b->gid;
}

caryll_buffer *caryll_write_gsub_ligature_subtable(otl_subtable *_subtable) {
	subtable_gsub_ligature *subtable = &(_subtable->gsub_ligature);

	ligature_aggerator *h = NULL, *s, *tmp;
	uint16_t nLigatures = subtable->to->numGlyphs;
	for (uint16_t j = 0; j < nLigatures; j++) {
		int sgid = subtable->from[j]->glyphs[0].index;
		HASH_FIND_INT(h, &sgid, s);
		if (!s) {
			NEW(s);
			s->gid = sgid;
			s->ligid = HASH_COUNT(h);
			HASH_ADD_INT(h, gid, s);
		}
	}
	HASH_SORT(h, by_gid);

	otl_coverage *startcov;
	NEW(startcov);
	startcov->numGlyphs = HASH_COUNT(h);
	NEW_N(startcov->glyphs, startcov->numGlyphs);

	uint16_t jj = 0;
	foreach_hash(s, h) {
		s->ligid = jj;
		startcov->glyphs[jj].index = s->gid;
		startcov->glyphs[jj].name = NULL;
		jj++;
	}

	caryll_bkblock *root = new_bkblock(b16, 1,                                                        // format
	                                   p16, new_bkblock_from_buffer(caryll_write_coverage(startcov)), // coverage
	                                   b16, startcov->numGlyphs,                                      // LigSetCount
	                                   bkover);

	foreach_hash(s, h) {
		uint16_t nLigsHere = 0;
		for (uint16_t j = 0; j < nLigatures; j++)
			if (subtable->from[j]->glyphs[0].index == s->gid) nLigsHere++;
		caryll_bkblock *ligset = new_bkblock(b16, nLigsHere, bkover);

		for (uint16_t j = 0; j < nLigatures; j++) {
			if (subtable->from[j]->glyphs[0].index == s->gid) {
				caryll_bkblock *ligdef = new_bkblock(b16, subtable->to->glyphs[j].index, // ligGlyph
				                                     b16, subtable->from[j]->numGlyphs,  // compCount
				                                     bkover);
				for (uint16_t m = 1; m < subtable->from[j]->numGlyphs; m++) {
					bkblock_push(ligdef, b16, subtable->from[j]->glyphs[m].index, bkover);
				}
				bkblock_push(ligset, p16, ligdef, bkover);
			}
		}
		bkblock_push(root, p16, ligset, bkover);
	}

	caryll_delete_coverage(startcov);
	HASH_ITER(hh, h, s, tmp) {
		HASH_DEL(h, s);
		free(s);
	}
	return caryll_write_bk(root);
}
