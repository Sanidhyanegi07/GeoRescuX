package com.example.georescux.data.sos

import android.content.SharedPreferences
import com.example.georescux.domain.sos.SosEmergency
import com.example.georescux.domain.sos.SosHistoryPolicy
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test

class SharedPreferencesSosStoreTest {

    private class FakeSharedPreferences : SharedPreferences {
        val map = mutableMapOf<String, Any?>()

        override fun getAll(): MutableMap<String, *> = HashMap(map)
        override fun getString(key: String, defValue: String?): String? = map[key] as? String ?: defValue
        override fun getStringSet(key: String, defValues: MutableSet<String>?): MutableSet<String>? =
            (map[key] as? Set<*>)?.filterIsInstance<String>()?.toMutableSet() ?: defValues
        override fun getInt(key: String, defValue: Int): Int = map[key] as? Int ?: defValue
        override fun getLong(key: String, defValue: Long): Long = map[key] as? Long ?: defValue
        override fun getFloat(key: String, defValue: Float): Float = map[key] as? Float ?: defValue
        override fun getBoolean(key: String, defValue: Boolean): Boolean = map[key] as? Boolean ?: defValue
        override fun contains(key: String): Boolean = map.containsKey(key)
        override fun edit(): SharedPreferences.Editor = FakeEditor(this)
        override fun registerOnSharedPreferenceChangeListener(listener: SharedPreferences.OnSharedPreferenceChangeListener?) {}
        override fun unregisterOnSharedPreferenceChangeListener(listener: SharedPreferences.OnSharedPreferenceChangeListener?) {}

        class FakeEditor(private val parent: FakeSharedPreferences) : SharedPreferences.Editor {
            private val modifications = mutableMapOf<String, Any?>()
            private val removals = mutableSetOf<String>()
            private var clearRequested = false

            override fun putString(key: String, value: String?): SharedPreferences.Editor {
                modifications[key] = value
                removals.remove(key)
                return this
            }
            override fun putStringSet(key: String, values: MutableSet<String>?): SharedPreferences.Editor {
                modifications[key] = values
                removals.remove(key)
                return this
            }
            override fun putInt(key: String, value: Int): SharedPreferences.Editor {
                modifications[key] = value
                removals.remove(key)
                return this
            }
            override fun putLong(key: String, value: Long): SharedPreferences.Editor {
                modifications[key] = value
                removals.remove(key)
                return this
            }
            override fun putFloat(key: String, value: Float): SharedPreferences.Editor {
                modifications[key] = value
                removals.remove(key)
                return this
            }
            override fun putBoolean(key: String, value: Boolean): SharedPreferences.Editor {
                modifications[key] = value
                removals.remove(key)
                return this
            }
            override fun remove(key: String): SharedPreferences.Editor {
                removals.add(key)
                modifications.remove(key)
                return this
            }
            override fun clear(): SharedPreferences.Editor {
                clearRequested = true
                return this
            }
            override fun commit(): Boolean {
                apply()
                return true
            }
            override fun apply() {
                if (clearRequested) parent.map.clear()
                for (key in removals) parent.map.remove(key)
                for ((key, value) in modifications) {
                    if (value == null) parent.map.remove(key)
                    else parent.map[key] = value
                }
            }
        }
    }

    private lateinit var fakePrefs: FakeSharedPreferences
    private lateinit var store: SharedPreferencesSosStore

    @Before
    fun setUp() {
        fakePrefs = FakeSharedPreferences()
        store = SharedPreferencesSosStore(fakePrefs)
    }

    @Test
    fun `0 records returns empty list`() {
        val history = store.loadHistory()
        assertTrue("Initial history must be empty", history.isEmpty())
        assertNull("Initial active must be null", store.loadActive())
    }

    @Test
    fun `1 record returns list with single emergency`() {
        val emergency = SosEmergency(id = "alert-1", startedAtMs = 1000L, note = "First help")
        store.addToHistory(emergency)

        val history = store.loadHistory()
        assertEquals(1, history.size)
        assertEquals("alert-1", history[0].id)
        assertEquals(1000L, history[0].startedAtMs)
        assertEquals("First help", history[0].note)
    }

    @Test
    fun `exactly 50 records preserves all records in newest-first order`() {
        for (i in 1..50) {
            store.addToHistory(SosEmergency(id = "alert-$i", startedAtMs = i * 1000L))
        }

        val history = store.loadHistory()
        assertEquals(50, history.size)
        assertEquals("alert-50", history.first().id)
        assertEquals(50000L, history.first().startedAtMs)
        assertEquals("alert-1", history.last().id)
        assertEquals(1000L, history.last().startedAtMs)
    }

    @Test
    fun `more than 50 records is capped at MAX_HISTORY`() {
        for (i in 1..75) {
            store.addToHistory(SosEmergency(id = "alert-$i", startedAtMs = i * 1000L))
        }

        val history = store.loadHistory()
        assertEquals(SosHistoryPolicy.MAX_HISTORY, history.size)
        assertEquals(50, history.size)
    }

    @Test
    fun `correct newest-record retention discards oldest records`() {
        for (i in 1..60) {
            store.addToHistory(SosEmergency(id = "alert-$i", startedAtMs = i * 1000L))
        }

        val history = store.loadHistory()
        assertEquals(50, history.size)
        // Newest record alert-60 (timestamp 60_000) must be first
        assertEquals("alert-60", history.first().id)
        assertEquals(60000L, history.first().startedAtMs)
        // 50th newest is alert-11 (timestamp 11_000)
        assertEquals("alert-11", history.last().id)
        assertEquals(11000L, history.last().startedAtMs)
        // Oldest records alert-1 through alert-10 must have been pruned
        val ids = history.map { it.id }.toSet()
        for (oldId in 1..10) {
            assertTrue("alert-$oldId must be discarded", "alert-$oldId" !in ids)
        }
    }

    @Test
    fun `existing active SOS preservation`() {
        val active = SosEmergency(id = "active-sos", startedAtMs = 999999L, note = "Currently trapped")
        store.saveActive(active)

        // Add 60 records to history which triggers retention capping
        for (i in 1..60) {
            store.addToHistory(SosEmergency(id = "hist-$i", startedAtMs = i * 1000L))
        }

        val loadedActive = store.loadActive()
        assertNotNull("Active SOS must survive history additions", loadedActive)
        assertEquals("active-sos", loadedActive!!.id)
        assertEquals("Currently trapped", loadedActive.note)

        // Clearing active does not affect history
        store.clearActive()
        assertNull(store.loadActive())
        assertEquals(50, store.loadHistory().size)
    }

    @Test
    fun `malformed stored JSON is handled safely`() {
        // Case 1: Corrupted non-JSON string in prefs
        fakePrefs.map["sos_history"] = "INVALID_CORRUPTED_JSON_STRING{{{"
        val history1 = store.loadHistory()
        assertTrue("Corrupted JSON must safely return empty list", history1.isEmpty())

        // Adding a new record recovers cleanly
        store.addToHistory(SosEmergency(id = "alert-recovered", startedAtMs = 5000L))
        val recovered = store.loadHistory()
        assertEquals(1, recovered.size)
        assertEquals("alert-recovered", recovered[0].id)

        // Case 2: Array containing an invalid object amongst valid ones
        val mixedJson = """[
            {"id":"valid-1","startedAtMs":1000,"stoppedAtMs":null,"location":null,"locationStatus":null,"note":null},
            {"corrupted_data":true},
            {"id":"valid-2","startedAtMs":2000,"stoppedAtMs":null,"location":null,"locationStatus":null,"note":null}
        ]""".trimIndent()
        fakePrefs.map["sos_history"] = mixedJson

        val history2 = store.loadHistory()
        assertEquals(2, history2.size)
        val validIds = history2.map { it.id }.toSet()
        assertTrue("valid-1" in validIds)
        assertTrue("valid-2" in validIds)
    }
}

