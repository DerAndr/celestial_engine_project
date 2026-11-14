package com.celestial.companion.domain.usecase

import android.content.Context
import android.util.Log
import com.celestial.companion.domain.model.SpellDefinition
import org.json.JSONArray
import org.json.JSONObject

private const val TAG = "GrimoireStorageManager"
private const val PREFS_NAME = "celestial_prefs"
private const val KEY_CUSTOM_SPELLS = "custom_spells"
private const val KEY_SPELL_VOICE_PREFS = "spell_voice_prefs"

/**
 * Decoupled persistence manager for Arcane Grimoire custom spells and per-spell voice chant preferences.
 */
class GrimoireStorageManager(private val context: Context?) {

    fun loadCustomSpells(): List<SpellDefinition> {
        val ctx = context ?: return emptyList()
        return try {
            val prefs = ctx.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
            val raw = prefs.getString(KEY_CUSTOM_SPELLS, null) ?: return emptyList()
            val arr = JSONArray(raw)
            val list = mutableListOf<SpellDefinition>()
            for (i in 0 until arr.length()) {
                list.add(SpellDefinition.fromJson(arr.getJSONObject(i)))
            }
            Log.i(TAG, "Loaded ${list.size} custom spells from Grimoire")
            list
        } catch (e: Exception) {
            Log.w(TAG, "Failed loading custom spells: ${e.message}")
            emptyList()
        }
    }

    fun persistCustomSpells(spells: List<SpellDefinition>) {
        val ctx = context ?: return
        try {
            val prefs = ctx.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
            val arr = JSONArray()
            spells.forEach { arr.put(it.toJson()) }
            prefs.edit().putString(KEY_CUSTOM_SPELLS, arr.toString()).apply()
        } catch (e: Exception) {
            Log.w(TAG, "Failed persisting custom spells: ${e.message}")
        }
    }

    fun loadVoicePreferences(): Map<String, Boolean> {
        val ctx = context ?: return emptyMap()
        return try {
            val prefs = ctx.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
            val raw = prefs.getString(KEY_SPELL_VOICE_PREFS, null) ?: return emptyMap()
            val json = JSONObject(raw)
            val map = mutableMapOf<String, Boolean>()
            val keys = json.keys()
            while (keys.hasNext()) {
                val k = keys.next()
                map[k] = json.optBoolean(k, true)
            }
            Log.i(TAG, "Loaded ${map.size} spell voice preferences")
            map
        } catch (e: Exception) {
            Log.w(TAG, "Failed loading voice prefs: ${e.message}")
            emptyMap()
        }
    }

    fun persistVoicePreferences(prefsMap: Map<String, Boolean>) {
        val ctx = context ?: return
        try {
            val prefs = ctx.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
            val json = JSONObject()
            prefsMap.forEach { (k, v) -> json.put(k, v) }
            prefs.edit().putString(KEY_SPELL_VOICE_PREFS, json.toString()).apply()
        } catch (e: Exception) {
            Log.w(TAG, "Failed persisting voice prefs: ${e.message}")
        }
    }
}
