package com.example.georescux.data.maps

import android.content.Context
import android.content.res.AssetManager
import com.example.georescux.domain.routing.MapRegion
import com.example.georescux.domain.routing.RouteGraph
import com.example.georescux.domain.routing.RouteEdge
import com.example.georescux.domain.routing.RouteNode
import java.io.File

/**
 * Loads a RouteGraph from the bundled map region asset (graph.json) or local package file.
 *
 * Parsing is strict: malformed or incomplete data returns null and the
 * repository falls back to the synthetic default graph — corrupted data is
 * never silently treated as valid.
 *
 * The JSON is parsed with the pure-Kotlin [MiniJson] reader so this class
 * stays testable on the JVM (org.json stubs throw in local unit tests).
 */
object RegionGraphLoader {

    fun fromAsset(assets: AssetManager, path: String): RouteGraph? = try {
        assets.open(path).use { stream -> parse(stream.bufferedReader().readText()) }
    } catch (e: Exception) {
        null
    }

    fun fromFile(file: File): RouteGraph? = try {
        if (!file.exists() || file.length() == 0L) null
        else file.inputStream().use { stream -> parse(stream.bufferedReader().readText()) }
    } catch (e: Exception) {
        null
    }

    fun loadForRegion(context: Context, region: MapRegion): RouteGraph? {
        if (region.graphAssetPath.isNotBlank()) {
            val fromAsset = fromAsset(context.assets, region.graphAssetPath)
            if (fromAsset != null) return fromAsset
        }
        val localFile = MapRegionCatalog.getGraphFile(context, region)
        return if (localFile != null) fromFile(localFile) else null
    }

    fun parse(json: String): RouteGraph? = try {
        val root = MiniJson.parse(json) as? Map<*, *> ?: return null
        val nodeEntries = root["nodes"] as? List<*> ?: return null
        val edgeEntries = root["edges"] as? List<*> ?: return null
        val region = root["region"] as? Map<*, *>
        val version = (region?.get("version") as? Double)?.toInt() ?: 0

        val nodes = nodeEntries.map { entry ->
            val map = entry as? Map<*, *> ?: throw IllegalArgumentException("invalid node entry")
            RouteNode(
                id = map["id"] as? String ?: error("node entry missing id"),
                latitude = map["latitude"] as? Double ?: error("node entry missing latitude"),
                longitude = map["longitude"] as? Double ?: error("node entry missing longitude"),
                isSafeHaven = map["isSafeHaven"] as? Boolean ?: false,
            )
        }
        val edges = edgeEntries.map { entry ->
            val map = entry as? Map<*, *> ?: throw IllegalArgumentException("invalid edge entry")
            RouteEdge(
                fromNodeId = map["fromNodeId"] as? String ?: error("edge entry missing fromNodeId"),
                toNodeId = map["toNodeId"] as? String ?: error("edge entry missing toNodeId"),
                distanceMeters = map["distanceMeters"] as? Double
                    ?: error("edge entry missing distanceMeters"),
            )
        }

        // An empty edges list is valid (nodes-only graph); an empty node
        // list is not — there would be nothing to route between.
        if (nodes.isEmpty()) null else RouteGraph(
            nodes = nodes,
            edges = edges,
            version = version,
        )
    } catch (e: Exception) {
        null
    }
}
