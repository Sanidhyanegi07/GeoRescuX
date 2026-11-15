package com.example.georescux.data.sos

import android.content.Context
import android.content.SharedPreferences
import com.example.georescux.domain.sos.SosEmergency
import com.example.georescux.domain.sos.SosHistoryPolicy
import org.json.JSONArray
import org.json.JSONObject

/**
 * SharedPreferences-backed SOS store — simple, reliable and fully offline.
 * Uses org.json, which is built into Android.
 * Enforces the bounded history retention policy ([SosHistoryPolicy.MAX_HISTORY] = 50)
 * to prevent unlimited storage growth while preserving active SOS records untouched.
 */
class SharedPreferencesSosStore(private val prefs: SharedPreferences) : LocalSosStore {

    private val storeLock = Any()

    constructor(context: Context) : this(
        context.getSharedPreferences("sos_store", Context.MODE_PRIVATE)
    )

    override fun loadActive(): SosEmergency? = synchronized(storeLock) {
        val json = prefs.getString(KEY_ACTIVE, null) ?: return null
        return runCatching { SosEmergencyJson.fromJson(JSONObject(json)) }.getOrNull()
    }

    override fun saveActive(emergency: SosEmergency) = synchronized(storeLock) {
        prefs.edit().putString(KEY_ACTIVE, SosEmergencyJson.toJson(emergency).toString()).apply()
    }

    override fun clearActive() = synchronized(storeLock) {
        prefs.edit().remove(KEY_ACTIVE).apply()
    }

    override fun loadHistory(): List<SosEmergency> = synchronized(storeLock) {
        val items = parseHistoryJson(prefs.getString(KEY_HISTORY, "[]"))
        return SosHistoryPolicy.keepNewest(items, SosHistoryPolicy.MAX_HISTORY)
    }

    override fun addToHistory(emergency: SosEmergency): Boolean = synchronized(storeLock) {
        replaceInHistory(emergency.id, emergency)
    }

    override fun replaceInHistory(replacedId: String, emergency: SosEmergency): Boolean = synchronized(storeLock) {
        val existing = parseHistoryJson(prefs.getString(KEY_HISTORY, "[]"))
        val updated = existing.filterNot { it.id == replacedId || it.id == emergency.id } + emergency
        val kept = SosHistoryPolicy.keepNewest(updated, SosHistoryPolicy.MAX_HISTORY)
        val array = JSONArray()
        // Store in chronological order (oldest first) so array[length - 1] is newest
        kept.asReversed().forEach {
            array.put(SosEmergencyJson.toJson(it))
        }
        val editor = prefs.edit().putString(KEY_HISTORY, array.toString())

        val isMainThread = runCatching {
            android.os.Looper.myLooper() != null && android.os.Looper.myLooper() == android.os.Looper.getMainLooper()
        }.getOrDefault(false)

        if (isMainThread) {
            editor.apply()
            true
        } else {
            editor.commit()
        }
    }

    override fun completeActive(completed: SosEmergency): Boolean = synchronized(storeLock) {
        val existing = parseHistoryJson(prefs.getString(KEY_HISTORY, "[]"))
        val updated = existing.filterNot { it.id == completed.id } + completed
        val kept = SosHistoryPolicy.keepNewest(updated, SosHistoryPolicy.MAX_HISTORY)
        val array = JSONArray()
        kept.asReversed().forEach {
            array.put(SosEmergencyJson.toJson(it))
        }
        val editor = prefs.edit()
            .remove(KEY_ACTIVE)
            .putString(KEY_HISTORY, array.toString())

        val isMainThread = runCatching {
            android.os.Looper.myLooper() != null && android.os.Looper.myLooper() == android.os.Looper.getMainLooper()
        }.getOrDefault(false)

        if (isMainThread) {
            editor.apply()
            true
        } else {
            editor.commit()
        }
    }

    private fun parseHistoryJson(rawJson: String?): List<SosEmergency> {
        if (rawJson.isNullOrBlank()) return emptyList()
        return runCatching {
            val array = JSONArray(rawJson)
            (0 until array.length()).mapNotNull { i ->
                runCatching {
                    val obj = array.optJSONObject(i) ?: return@mapNotNull null
                    SosEmergencyJson.fromJson(obj)
                }.getOrNull()
            }
        }.getOrDefault(emptyList())
    }

    private companion object {
        const val KEY_ACTIVE = "active_emergency"
        const val KEY_HISTORY = "sos_history"
    }
}
