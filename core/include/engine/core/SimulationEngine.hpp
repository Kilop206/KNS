@@
         int getNextHop(int current, int destination) const;
 
         Topology& getTopology();
 
         const Topology& getTopology() const;
@@
         int getPacketsPerRoute() const;
 
         ValidationReport validateSimulation() const;
 
         void advanceTime(double time);
+
+        // GUI / topology modification helpers
+        int createNode();
+        bool deleteNode(int id);
+        Topology::LinkPtr createLink(int a, int b, double bandwidth_mbps, double delay_ms, double loss_prob = 0.0, LinkMode mode = LinkMode::FULL_DUPLEX);
+        bool deleteLink(int a, int b);
+        bool toggleLinkUp(int a, int b, bool up);
+        void rebuildRoutingTables();
     };
 }
