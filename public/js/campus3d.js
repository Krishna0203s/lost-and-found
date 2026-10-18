/**
 * Campus Lost & Found — 3D Campus Spatial Digital Twin & Item Matrix
 * High-performance WebGL 3D visualization using Three.js.
 * Features:
 * - 3D Holographic Campus Digital Twin with architectural zone pedestals
 * - Floating glowing 3D crystals for Lost (Amber), Found (Emerald), and Matched (Cyan)
 * - Dynamic 3D curved laser linkages between matched items
 * - Clean static location pedestal bases
 * - Swirling ambient particle starfield
 * - Smooth touch & mouse orbit controls with momentum damping
 * - Raycaster node inspection with interactive floating 3D HUD tooltip
 */

class Campus3DScene {
    constructor(containerId) {
        this.container = document.getElementById(containerId);
        if (!this.container || typeof THREE === 'undefined') {
            console.warn('Campus3D: Container or THREE not found');
            return;
        }

        this.items = [];
        this.matches = [];
        this.itemMeshes = [];
        this.laserMeshes = [];
        this.pulseRings = [];
        this.hoveredItem = null;
        this.autoRotate = true;
        this.viewMode = 'ISO'; // 'ISO', 'TOP', 'MATCHES'

        // Campus Zones definition with 3D coordinates
        this.zones = [
            { id: 'library', name: 'Campus Library', short: 'Library 2nd Fl', x: -15, z: -11, color: 0x3b82f6, icon: '🏛️' },
            { id: 'engineering', name: 'Engineering Block', short: 'Eng. Lab 3', x: 15, z: -11, color: 0x8b5cf6, icon: '💻' },
            { id: 'cafeteria', name: 'Cafeteria Main Hall', short: 'Dining Hall', x: 0, z: -2, color: 0xf59e0b, icon: '☕' },
            { id: 'science', name: 'Science Block', short: 'Sci. Rm 204', x: -14, z: 13, color: 0x10b981, icon: '🔬' },
            { id: 'gym', name: 'Campus Gym', short: 'Gym Locker Area', x: 15, z: 13, color: 0xef4444, icon: '🏋️' },
            { id: 'gate', name: 'Main Gate', short: 'Parking Lot', x: 0, z: 20, color: 0x06b6d4, icon: '🚗' }
        ];

        // Orbit camera state
        this.isDragging = false;
        this.prevPointerPos = { x: 0, y: 0 };
        this.spherical = { radius: 52, theta: Math.PI / 4, phi: Math.PI / 3.2 };
        this.targetSpherical = { radius: 52, theta: Math.PI / 4, phi: Math.PI / 3.2 };
        this.center = new THREE.Vector3(0, 0, 0);

        this.init();
    }

    init() {
        const width = this.container.clientWidth || 800;
        const height = this.container.clientHeight || 460;

        // Scene
        this.scene = new THREE.Scene();
        this.scene.fog = new THREE.FogExp2(0x0a0f18, 0.012);

        // Camera
        this.camera = new THREE.PerspectiveCamera(45, width / height, 1, 1000);
        this.updateCameraPos();

        // Renderer
        this.renderer = new THREE.WebGLRenderer({ antialias: true, alpha: true, powerPreference: 'high-performance' });
        this.renderer.setSize(width, height);
        this.renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
        this.renderer.toneMapping = THREE.ACESFilmicToneMapping;
        this.renderer.toneMappingExposure = 1.25;
        this.container.appendChild(this.renderer.domElement);

        // Raycaster for hover/click interaction
        this.raycaster = new THREE.Raycaster();
        this.pointer = new THREE.Vector2(-999, -999);

        // Lighting
        this.setupLights();

        // Environment: Grid, Campus Pedestals, Ambient Cyber Particles
        this.buildCampusEnvironment();
        this.buildParticles();

        // Events
        this.bindEvents();

        // Animation Loop
        this.clock = new THREE.Clock();
        this.animate = this.animate.bind(this);
        requestAnimationFrame(this.animate);
    }

    setupLights() {
        // Ambient soft cyan fill
        const ambientLight = new THREE.AmbientLight(0x0e2439, 2.2);
        this.scene.add(ambientLight);

        // Key overhead light
        const mainLight = new THREE.DirectionalLight(0xdff9fb, 1.8);
        mainLight.position.set(30, 60, 40);
        this.scene.add(mainLight);

        // Core central glowing point light
        this.coreLight = new THREE.PointLight(0x00f5a0, 2.5, 75);
        this.coreLight.position.set(0, 8, 0);
        this.scene.add(this.coreLight);

        // Secondary cyan rim light
        const rimLight = new THREE.DirectionalLight(0x00d4ff, 1.2);
        rimLight.position.set(-40, 20, -30);
        this.scene.add(rimLight);
    }

    buildCampusEnvironment() {
        // 1. Holographic Baseplate Grid
        const gridHelper = new THREE.GridHelper(64, 32, 0x00f5a0, 0x133044);
        gridHelper.position.y = -0.05;
        this.scene.add(gridHelper);

        // 2. Concentric Holographic Distance Circles
        for (let r = 10; r <= 32; r += 7) {
            const circleGeo = new THREE.RingGeometry(r - 0.08, r, 64);
            const circleMat = new THREE.MeshBasicMaterial({
                color: 0x00d4ff,
                transparent: true,
                opacity: 0.18,
                side: THREE.DoubleSide
            });
            const ring = new THREE.Mesh(circleGeo, circleMat);
            ring.rotation.x = Math.PI / 2;
            ring.position.y = 0.01;
            this.scene.add(ring);
        }

        // 3. Central Hub Hologram Compass Disc
        const discGeo = new THREE.CylinderGeometry(4.5, 4.5, 0.4, 32);
        const discMat = new THREE.MeshStandardMaterial({
            color: 0x0b1726,
            metalness: 0.8,
            roughness: 0.2,
            emissive: 0x003322,
            emissiveIntensity: 0.5
        });
        const disc = new THREE.Mesh(discGeo, discMat);
        disc.position.y = -0.2;
        this.scene.add(disc);

        const innerRingGeo = new THREE.RingGeometry(4.2, 4.4, 32);
        const innerRingMat = new THREE.MeshBasicMaterial({
            color: 0x00f5a0,
            transparent: true,
            opacity: 0.8,
            side: THREE.DoubleSide
        });
        const innerRing = new THREE.Mesh(innerRingGeo, innerRingMat);
        innerRing.rotation.x = Math.PI / 2;
        innerRing.position.y = 0.02;
        this.scene.add(innerRing);

        // 4. Campus Architectural Zone Pedestals
        this.zoneMeshes = [];
        this.zones.forEach(z => {
            const group = new THREE.Group();
            group.position.set(z.x, 0, z.z);

            // Hexagonal Platform
            const platGeo = new THREE.CylinderGeometry(4.2, 4.5, 0.8, 6);
            const platMat = new THREE.MeshStandardMaterial({
                color: 0x0f1b2b,
                metalness: 0.6,
                roughness: 0.3,
                emissive: z.color,
                emissiveIntensity: 0.22
            });
            const plat = new THREE.Mesh(platGeo, platMat);
            plat.position.y = 0.3;
            group.add(plat);

            // Platform Glowing Neon Edge Ring
            const rimGeo = new THREE.RingGeometry(3.9, 4.2, 6);
            const rimMat = new THREE.MeshBasicMaterial({
                color: z.color,
                transparent: true,
                opacity: 0.85,
                side: THREE.DoubleSide
            });
            const rim = new THREE.Mesh(rimGeo, rimMat);
            rim.rotation.x = Math.PI / 2;
            rim.position.y = 0.72;
            group.add(rim);

            // Zone Holographic Pillar Marker
            const pillarGeo = new THREE.CylinderGeometry(0.3, 0.3, 3.5, 12);
            const pillarMat = new THREE.MeshStandardMaterial({
                color: z.color,
                metalness: 0.9,
                roughness: 0.1,
                emissive: z.color,
                emissiveIntensity: 0.8
            });
            const pillar = new THREE.Mesh(pillarGeo, pillarMat);
            pillar.position.y = 2.2;
            group.add(pillar);

            // Zone floating icon sprite / marker
            const sprite = this.createZoneLabelSprite(z.icon, z.short, z.color);
            sprite.position.y = 5.2;
            group.add(sprite);

            this.scene.add(group);
            this.zoneMeshes.push({ zone: z, group });
        });
    }

    createZoneLabelSprite(icon, label, colorHex) {
        const canvas = document.createElement('canvas');
        canvas.width = 256;
        canvas.height = 80;
        const ctx = canvas.getContext('2d');

        // Draw pill background
        ctx.fillStyle = 'rgba(8, 18, 30, 0.85)';
        ctx.strokeStyle = `#${colorHex.toString(16).padStart(6, '0')}`;
        ctx.lineWidth = 3;
        ctx.beginPath();
        ctx.roundRect(8, 8, 240, 64, 32);
        ctx.fill();
        ctx.stroke();

        // Text
        ctx.font = 'bold 26px "Space Grotesk", sans-serif';
        ctx.fillStyle = '#ffffff';
        ctx.textAlign = 'center';
        ctx.textBaseline = 'middle';
        ctx.fillText(`${icon} ${label}`, 128, 40);

        const texture = new THREE.CanvasTexture(canvas);
        texture.minFilter = THREE.LinearFilter;
        const mat = new THREE.SpriteMaterial({ map: texture, transparent: true, depthTest: false });
        const sprite = new THREE.Sprite(mat);
        sprite.scale.set(7.5, 2.4, 1);
        return sprite;
    }

    buildParticles() {
        const count = 420;
        const geo = new THREE.BufferGeometry();
        const positions = new Float32Array(count * 3);
        const colors = new Float32Array(count * 3);

        const c1 = new THREE.Color(0x00f5a0);
        const c2 = new THREE.Color(0x00d4ff);
        const c3 = new THREE.Color(0xffaa00);

        for (let i = 0; i < count; i++) {
            positions[i * 3] = (Math.random() - 0.5) * 65;
            positions[i * 3 + 1] = Math.random() * 26 + 0.5;
            positions[i * 3 + 2] = (Math.random() - 0.5) * 65;

            const chosen = Math.random() < 0.4 ? c1 : (Math.random() < 0.7 ? c2 : c3);
            colors[i * 3] = chosen.r;
            colors[i * 3 + 1] = chosen.g;
            colors[i * 3 + 2] = chosen.b;
        }

        geo.setAttribute('position', new THREE.BufferAttribute(positions, 3));
        geo.setAttribute('color', new THREE.BufferAttribute(colors, 3));

        const mat = new THREE.PointsMaterial({
            size: 0.65,
            vertexColors: true,
            transparent: true,
            opacity: 0.75,
            blending: THREE.AdditiveBlending
        });

        this.particles = new THREE.Points(geo, mat);
        this.scene.add(this.particles);
    }

    // Radar sweep removed per user request (ugly green radar/beacon removed)

    // =========================================================================
    // DYNAMIC DATA SYNCHRONIZATION
    // =========================================================================
    setItemsAndMatches(items, matches = []) {
        this.items = items || [];
        this.matches = matches || [];

        // Clear existing item meshes and lasers
        this.itemMeshes.forEach(m => this.scene.remove(m.group));
        this.itemMeshes = [];

        this.laserMeshes.forEach(l => this.scene.remove(l));
        this.laserMeshes = [];

        // Map items to 3D positions
        this.items.forEach((item, idx) => {
            const pos = this.calculateItemPosition(item, idx);
            const itemMeshObj = this.createItemNodeMesh(item, pos);
            this.scene.add(itemMeshObj.group);
            this.itemMeshes.push(itemMeshObj);
        });

        // Create 3D Laser Linkages for strong matches
        this.buildMatchLasers();

        // Update telemetry HUD
        this.updateHUDStats();
    }

    calculateItemPosition(item, idx) {
        // Find matching zone based on item location string
        const locLower = (item.location || '').toLowerCase();
        let targetZone = this.zones.find(z => locLower.includes(z.id) || locLower.includes(z.name.toLowerCase()));

        if (!targetZone) {
            if (locLower.includes('lib')) targetZone = this.zones[0];
            else if (locLower.includes('eng') || locLower.includes('lab 3')) targetZone = this.zones[1];
            else if (locLower.includes('cafe') || locLower.includes('dining')) targetZone = this.zones[2];
            else if (locLower.includes('sci')) targetZone = this.zones[3];
            else if (locLower.includes('gym')) targetZone = this.zones[4];
            else if (locLower.includes('gate') || locLower.includes('park')) targetZone = this.zones[5];
            else targetZone = this.zones[idx % this.zones.length];
        }

        // Pseudo-random offset around the zone pedestal so items don't collide
        const angle = ((item.item_id * 137 + idx * 79) % 360) * (Math.PI / 180);
        const radius = 1.2 + ((item.item_id * 23) % 18) / 10;
        const x = targetZone.x + Math.cos(angle) * radius;
        const z = targetZone.z + Math.sin(angle) * radius;
        const y = 2.8 + ((item.item_id * 17) % 12) / 10;

        return new THREE.Vector3(x, y, z);
    }

    createItemNodeMesh(item, pos) {
        const group = new THREE.Group();
        group.position.copy(pos);

        const isLost = item.item_type === 'LOST';
        const isMatched = item.status === 'MATCHED';
        const isReturned = item.status === 'RETURNED';

        let colorHex = isLost ? 0xff9900 : 0x00f5a0;
        if (isMatched) colorHex = 0x00d4ff;
        if (isReturned) colorHex = 0x10b981;

        // 1. Crystal Geometry based on status
        let geo;
        if (isMatched) {
            geo = new THREE.IcosahedronGeometry(0.85, 0);
        } else if (isLost) {
            geo = new THREE.OctahedronGeometry(0.8, 0);
        } else {
            geo = new THREE.DodecahedronGeometry(0.75, 0);
        }

        const mat = new THREE.MeshStandardMaterial({
            color: colorHex,
            emissive: colorHex,
            emissiveIntensity: 0.7,
            metalness: 0.9,
            roughness: 0.1,
            wireframe: false
        });

        const mesh = new THREE.Mesh(geo, mat);
        mesh.userData = { item, isItemNode: true };
        group.add(mesh);

        // 2. Vertical Laser Light Projection to Ground
        const beamGeo = new THREE.BufferGeometry().setFromPoints([
            new THREE.Vector3(0, 0, 0),
            new THREE.Vector3(0, -pos.y, 0)
        ]);
        const beamMat = new THREE.LineBasicMaterial({
            color: colorHex,
            transparent: true,
            opacity: 0.4
        });
        const beam = new THREE.Line(beamGeo, beamMat);
        group.add(beam);

        // 3. Subtle static floor indicator
        const groundRingGeo = new THREE.RingGeometry(0.7, 0.9, 24);
        const groundRingMat = new THREE.MeshBasicMaterial({
            color: colorHex,
            transparent: true,
            opacity: 0.3,
            side: THREE.DoubleSide
        });
        const groundRing = new THREE.Mesh(groundRingGeo, groundRingMat);
        groundRing.rotation.x = Math.PI / 2;
        groundRing.position.y = -pos.y + 0.05;
        group.add(groundRing);

        return {
            group,
            mesh,
            item,
            initialY: pos.y,
            speed: 1.5 + (item.item_id % 5) * 0.3,
            rotSpeed: 0.02 + (item.item_id % 3) * 0.01
        };
    }

    buildMatchLasers() {
        if (!this.matches || this.matches.length === 0) return;

        this.matches.forEach(m => {
            if (!m.is_possible_match && m.total_score < 70) return;

            const lostNode = this.itemMeshes.find(im => im.item.item_id === m.lost_item_id);
            const foundNode = this.itemMeshes.find(im => im.item.item_id === m.found_item_id);

            if (lostNode && foundNode) {
                const start = lostNode.group.position;
                const end = foundNode.group.position;
                const mid = new THREE.Vector3()
                    .addVectors(start, end)
                    .multiplyScalar(0.5);
                mid.y += 6.5; // Arc upward in 3D

                const curve = new THREE.QuadraticBezierCurve3(start, mid, end);
                const points = curve.getPoints(36);
                const geo = new THREE.BufferGeometry().setFromPoints(points);

                const mat = new THREE.LineBasicMaterial({
                    color: 0x00d4ff,
                    linewidth: 3,
                    transparent: true,
                    opacity: 0.85
                });

                const line = new THREE.Line(geo, mat);
                this.scene.add(line);
                this.laserMeshes.push(line);
            }
        });
    }

    // =========================================================================
    // INTERACTION & CAMERA CONTROLS
    // =========================================================================
    bindEvents() {
        const dom = this.renderer.domElement;

        // Pointer down
        dom.addEventListener('pointerdown', (e) => {
            this.isDragging = true;
            this.prevPointerPos = { x: e.clientX, y: e.clientY };
            this.autoRotate = false; // pause auto rotation during interaction
            const autoBtn = document.getElementById('btn3dAutoRotate');
            if (autoBtn) autoBtn.classList.remove('active');
        });

        // Pointer move (orbit drag & raycast)
        window.addEventListener('pointermove', (e) => {
            const rect = dom.getBoundingClientRect();

            if (this.isDragging) {
                const dx = e.clientX - this.prevPointerPos.x;
                const dy = e.clientY - this.prevPointerPos.y;

                this.targetSpherical.theta -= dx * 0.008;
                this.targetSpherical.phi -= dy * 0.008;
                // Clamp phi to prevent flipping
                this.targetSpherical.phi = Math.max(0.15, Math.min(Math.PI / 2 - 0.05, this.targetSpherical.phi));

                this.prevPointerPos = { x: e.clientX, y: e.clientY };
            }

            // Raycasting coordinate normalization
            if (e.clientX >= rect.left && e.clientX <= rect.right &&
                e.clientY >= rect.top && e.clientY <= rect.bottom) {
                this.pointer.x = ((e.clientX - rect.left) / rect.width) * 2 - 1;
                this.pointer.y = -((e.clientY - rect.top) / rect.height) * 2 + 1;
                this.checkRaycastHover(e);
            } else {
                this.hideTooltip();
            }
        });

        // Pointer up
        window.addEventListener('pointerup', () => {
            this.isDragging = false;
        });

        // Scroll to zoom
        dom.addEventListener('wheel', (e) => {
            e.preventDefault();
            this.targetSpherical.radius += e.deltaY * 0.035;
            this.targetSpherical.radius = Math.max(22, Math.min(85, this.targetSpherical.radius));
        }, { passive: false });

        // Node click: open item modal
        dom.addEventListener('click', (e) => {
            if (this.hoveredItem) {
                if (typeof window.openItemDetailsModal === 'function') {
                    window.openItemDetailsModal(this.hoveredItem.item_id);
                }
            }
        });

        // Window resize
        window.addEventListener('resize', () => this.handleResize());
    }

    checkRaycastHover(e) {
        this.raycaster.setFromCamera(this.pointer, this.camera);
        const interactableMeshes = this.itemMeshes.map(m => m.mesh);
        const intersects = this.raycaster.intersectObjects(interactableMeshes, false);

        if (intersects.length > 0) {
            const hit = intersects[0].object;
            const item = hit.userData.item;
            this.hoveredItem = item;
            this.renderer.domElement.style.cursor = 'pointer';

            // Highlight hit mesh
            hit.scale.set(1.4, 1.4, 1.4);

            this.showTooltip(item, e.clientX, e.clientY);
        } else {
            if (this.hoveredItem) {
                // Reset scales
                this.itemMeshes.forEach(m => m.mesh.scale.set(1, 1, 1));
            }
            this.hoveredItem = null;
            this.renderer.domElement.style.cursor = 'grab';
            this.hideTooltip();
        }
    }

    showTooltip(item, mouseX, mouseY) {
        let tooltip = document.getElementById('campus3dTooltip');
        if (!tooltip) {
            tooltip = document.createElement('div');
            tooltip.id = 'campus3dTooltip';
            tooltip.className = 'campus-3d-tooltip glass-panel';
            document.body.appendChild(tooltip);
        }

        const isLost = item.item_type === 'LOST';
        const badgeClass = isLost ? 'pill-lost' : 'pill-found';
        const statusBadge = `<span class="badge badge-${item.status.toLowerCase()}">${item.status}</span>`;

        tooltip.innerHTML = `
            <div class="tooltip-header">
                <span class="pill-btn ${badgeClass}" style="padding:2px 8px; font-size:0.75rem;">${item.item_type}</span>
                ${statusBadge}
            </div>
            <h4 class="tooltip-title">${item.item_name}</h4>
            <div class="tooltip-meta">
                <span>📍 ${item.location}</span>
                <span>🏷️ ${item.category}</span>
                <span>⚡ ${item.priority}</span>
            </div>
            <div class="tooltip-footer">Click 3D item to view full record →</div>
        `;

        tooltip.style.display = 'block';
        tooltip.style.left = `${mouseX + 16}px`;
        tooltip.style.top = `${mouseY + 16}px`;
    }

    hideTooltip() {
        const tooltip = document.getElementById('campus3dTooltip');
        if (tooltip) tooltip.style.display = 'none';
    }

    setViewMode(mode) {
        this.viewMode = mode;
        if (mode === 'ISO') {
            this.targetSpherical = { radius: 52, theta: Math.PI / 4, phi: Math.PI / 3.2 };
        } else if (mode === 'TOP') {
            this.targetSpherical = { radius: 55, theta: 0, phi: 0.15 };
        } else if (mode === 'MATCHES') {
            // Find active match centroid
            if (this.laserMeshes.length > 0) {
                this.targetSpherical = { radius: 36, theta: Math.PI / 3, phi: Math.PI / 3.6 };
            } else {
                this.targetSpherical = { radius: 46, theta: Math.PI / 2, phi: Math.PI / 3.5 };
            }
        }
    }

    toggleAutoRotate() {
        this.autoRotate = !this.autoRotate;
        return this.autoRotate;
    }

    handleResize() {
        const width = this.container.clientWidth;
        const height = this.container.clientHeight;
        if (!width || !height) return;

        this.camera.aspect = width / height;
        this.camera.updateProjectionMatrix();
        this.renderer.setSize(width, height);
    }

    updateCameraPos() {
        // Convert spherical coords to Cartesian
        const { radius, theta, phi } = this.spherical;
        this.camera.position.x = radius * Math.sin(phi) * Math.sin(theta);
        this.camera.position.y = radius * Math.cos(phi);
        this.camera.position.z = radius * Math.sin(phi) * Math.cos(theta);
        this.camera.lookAt(this.center);
    }

    updateHUDStats() {
        const hudNodes = document.getElementById('hud3dNodesCount');
        if (hudNodes) hudNodes.textContent = `${this.items.length} 3D Items`;

        const hudLasers = document.getElementById('hud3dLasersCount');
        if (hudLasers) hudLasers.textContent = `${this.laserMeshes.length} Laser Links`;
    }

    // =========================================================================
    // RENDER & ANIMATION LOOP
    // =========================================================================
    animate() {
        requestAnimationFrame(this.animate);
        const delta = this.clock.getDelta();
        const time = this.clock.getElapsedTime();

        // Auto rotate when enabled
        if (this.autoRotate && !this.isDragging) {
            this.targetSpherical.theta += 0.003;
        }

        // Camera damping
        this.spherical.radius += (this.targetSpherical.radius - this.spherical.radius) * 0.08;
        this.spherical.theta += (this.targetSpherical.theta - this.spherical.theta) * 0.08;
        this.spherical.phi += (this.targetSpherical.phi - this.spherical.phi) * 0.08;
        this.updateCameraPos();

        // Animate Item crystals (floating bob + slow spin)
        this.itemMeshes.forEach(im => {
            im.mesh.rotation.y += im.rotSpeed;
            im.mesh.rotation.x = Math.sin(time * im.speed) * 0.15;
            im.group.position.y = im.initialY + Math.sin(time * im.speed) * 0.35;
        });

        // Animate ambient cyber dust particles
        if (this.particles) {
            this.particles.rotation.y = time * 0.04;
        }

        // Pulse core light
        if (this.coreLight) {
            this.coreLight.intensity = 2.0 + Math.sin(time * 2) * 0.6;
        }

        this.renderer.render(this.scene, this.camera);
    }
}

window.Campus3DScene = Campus3DScene;
