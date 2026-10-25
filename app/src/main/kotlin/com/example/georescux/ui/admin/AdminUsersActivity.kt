package com.example.georescux.ui.admin

import android.os.Bundle
import android.text.Editable
import android.view.View
import android.widget.Button
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import androidx.core.widget.addTextChangedListener
import androidx.lifecycle.lifecycleScope
import com.example.georescux.GeoRescuXApplication
import com.example.georescux.R
import com.example.georescux.data.auth.UserRole
import com.example.georescux.data.maps.MapRegionCatalog
import com.google.firebase.database.DataSnapshot
import com.google.firebase.database.DatabaseError
import com.google.firebase.database.FirebaseDatabase
import com.google.firebase.database.ValueEventListener
import kotlinx.coroutines.launch

/**
 * Administrative user directory (spec §51).
 *
 * Lists accounts known to the backend with the fields an administrator is
 * permitted to see: email, region and verification state plus a per-user SOS
 * count. Secrets, password material and identity documents are never shown
 * here — documents stay inside the authorized verification workflow only.
 */
class AdminUsersActivity : AppCompatActivity() {

    private val database by lazy { FirebaseDatabase.getInstance() }
    private lateinit var listContainer: LinearLayout
    private lateinit var summaryText: TextView

    private data class AdminUserRow(
        val uid: String,
        val email: String,
        val name: String?,
        val regionId: String?,
        val isVerified: Boolean,
        val sosCount: Int,
    )

    private val profileMap = mutableMapOf<String, Triple<String, String?, String?>>()
    private val sosMap = mutableMapOf<String, Pair<Boolean, Int>>()
    private val users = mutableListOf<AdminUserRow>()
    private var filter: String = ""
    private var usersListener: ValueEventListener? = null
    private var sosListener: ValueEventListener? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val appContainer = (application as GeoRescuXApplication).appContainer
        lifecycleScope.launch {
            val role = appContainer.roleResolver.resolveRole()
            if (role != UserRole.ADMIN) {
                Toast.makeText(this@AdminUsersActivity, "Access Denied: Admin authorization required", Toast.LENGTH_LONG).show()
                finish()
                return@launch
            }
        }

        setContentView(R.layout.activity_admin_users)

        listContainer = findViewById(R.id.usersListContainer)
        summaryText = findViewById(R.id.textUsersSummary)

        findViewById<View>(R.id.buttonAdminUsersBack).setOnClickListener { finish() }
        findViewById<EditText>(R.id.editUserSearch).addTextChangedListener { editable: Editable? ->
            filter = editable?.toString().orEmpty().trim().lowercase()
            renderUsers()
        }

        usersListener = object : ValueEventListener {
            override fun onDataChange(snapshot: DataSnapshot) {
                profileMap.clear()
                for (child in snapshot.children) {
                    val uid = child.key ?: continue
                    val email = child.child("email").getValue(String::class.java) ?: "(no email)"
                    val name = child.child("name").getValue(String::class.java)
                    val regionId = child.child("regionId").getValue(String::class.java)
                    profileMap[uid] = Triple(email, name, regionId)
                }
                recomputeAndRenderUsers()
            }

            override fun onCancelled(error: DatabaseError) {}
        }

        sosListener = object : ValueEventListener {
            override fun onDataChange(snapshot: DataSnapshot) {
                sosMap.clear()
                for (userSnap in snapshot.children) {
                    val uid = userSnap.key ?: continue
                    val isVerified = userSnap.child("isVerified").getValue(Boolean::class.java) ?: false
                    var sosCount = 0
                    for (alertSnap in userSnap.children) {
                        if (alertSnap.key != "isVerified") {
                            sosCount++
                        }
                    }
                    sosMap[uid] = Pair(isVerified, sosCount)
                }
                recomputeAndRenderUsers()
            }

            override fun onCancelled(error: DatabaseError) {}
        }

        database.getReference("users").addValueEventListener(usersListener!!)
        database.getReference("sos_alerts").addValueEventListener(sosListener!!)
    }

    private fun recomputeAndRenderUsers() {
        val allUids = (profileMap.keys + sosMap.keys).toSet()
        users.clear()
        for (uid in allUids) {
            val prof = profileMap[uid]
            val sos = sosMap[uid]
            val email = prof?.first ?: "(registered user)"
            val name = prof?.second
            val regionId = prof?.third
            val isVerified = sos?.first ?: false
            val count = sos?.second ?: 0
            users.add(AdminUserRow(uid, email, name, regionId, isVerified, count))
        }
        renderUsers()
    }

    private fun renderUsers() {
        listContainer.removeAllViews()
        val matched = users.filter {
            filter.isBlank() ||
                it.email.lowercase().contains(filter) ||
                (it.name?.lowercase()?.contains(filter) == true) ||
                it.uid.lowercase().contains(filter)
        }
        val verified = matched.count { it.isVerified }
        summaryText.text = "${matched.size} account(s) in view · $verified verified"

        if (matched.isEmpty()) {
            TextView(this).apply {
                text = "No users match."
                setTextColor(ContextCompat.getColor(this@AdminUsersActivity, R.color.text_muted))
                textSize = 13f
                setPadding(24, 24, 24, 24)
            }.also { listContainer.addView(it) }
            return
        }

        matched.sortedByDescending { it.sosCount }.forEach { user ->
            val regionName = user.regionId?.let { MapRegionCatalog.byId(it)?.displayName }
            TextView(this).apply {
                text = buildString {
                    val displayName = if (!user.name.isNullOrBlank()) "${user.name} (${user.email})" else user.email
                    append("👤 $displayName\n")
                    append("UID: ${user.uid.take(12)}… · ")
                    append("Region: ${regionName ?: "Default Region"}\n")
                    append("Verification: ")
                    append(if (user.isVerified) "VERIFIED (×2 priority)" else "unverified (×1 priority)")
                    append(" · SOS records: ${user.sosCount}")
                }
                setTextColor(ContextCompat.getColor(this@AdminUsersActivity, R.color.text_primary))
                textSize = 13f
                setPadding(28, 24, 28, 24)
                background = ContextCompat.getDrawable(this@AdminUsersActivity, R.drawable.bg_card_white)
            }.also { 
                val params = LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT)
                params.setMargins(0, 0, 0, 16)
                it.layoutParams = params
                listContainer.addView(it) 
            }
        }
    }

    override fun onDestroy() {
        usersListener?.let { database.getReference("users").removeEventListener(it) }
        sosListener?.let { database.getReference("sos_alerts").removeEventListener(it) }
        super.onDestroy()
    }
}
