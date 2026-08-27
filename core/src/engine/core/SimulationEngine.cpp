@@
     }
 
     void SimulationEngine::advanceTime(double time) {
         clock_.setTime(time);
     }
+
+    int SimulationEngine::createNode() {
+        const int id = topology_.addNode();
+        rebuildRoutingTables();
+        return id;
+    }
+
+    bool SimulationEngine::deleteNode(int id) {
+        const bool ok = topology_.removeNode(id);
+        if (ok) rebuildRoutingTables();
+        return ok;
+    }
+
+    Topology::LinkPtr SimulationEngine::createLink(int a, int b, double bandwidth_mbps, double delay_ms, double loss_prob, LinkMode mode) {
+        auto ptr = topology_.addLinkPtr(a, b, bandwidth_mbps, delay_ms, loss_prob, mode);
+        rebuildRoutingTables();
+        return ptr;
+    }
+
+    bool SimulationEngine::deleteLink(int a, int b) {
+        const bool ok = topology_.removeLink(a, b);
+        if (ok) rebuildRoutingTables();
+        return ok;
+    }
+
+    bool SimulationEngine::toggleLinkUp(int a, int b, bool up) {
+        const bool ok = topology_.setLinkUp(a, b, up);
+        if (ok) rebuildRoutingTables();
+        return ok;
+    }
+
+    void SimulationEngine::rebuildRoutingTables() {
+        const int n = topology_.size();
+        routing_tables_.clear();
+        routing_tables_.resize(n);
+        Routing routing;
+        for (int u = 0; u < n; ++u) {
+            routing_tables_[u] = routing.buildRoutingTable(topology_, u);
+        }
+    }
 }
