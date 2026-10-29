/**
 * Celestial Engine Mk.V — Optional Dynamically-Imported 3D Replica View
 * Uses Three.js (loaded on-demand only when the user enables 3D View).
 * Shares the exact same 43 LED states and 128x64 OLED canvas texture as the 2D simulator.
 */

const THREE_CDN = 'https://cdn.jsdelivr.net/npm/three@0.170.0/build/three.module.min.js';

export class Staff3DRenderer {
  constructor(container, oledCanvas, store, onSelectHotspot) {
    this.container = container;
    this.oledCanvas = oledCanvas;
    this.store = store;
    this.onSelectHotspot = onSelectHotspot || (() => {});
    this.THREE = null;
    this.renderer = null;
    this.scene = null;
    this.camera = null;
    this.staffGroup = null;
    this.ledMeshes = [];
    this.oledTexture = null;
    this.interactiveMeshes = [];
    this.isInitialized = false;
    this.isDragging = false;
    this.prevPointer = { x: 0, y: 0 };
    this.rotY = -0.25;
    this.rotX = 0.12;
    this.zoomDist = 6.2;
  }

  async init() {
    if (this.isInitialized) return true;
    if (!this.container) return false;

    const THREE = await import(THREE_CDN);
    this.THREE = THREE;

    const width = this.container.clientWidth || 460;
    const height = this.container.clientHeight || 490;

    this.scene = new THREE.Scene();

    this.camera = new THREE.PerspectiveCamera(38, width / height, 0.1, 100);
    this.camera.position.set(0, 0.2, this.zoomDist);

    this.renderer = new THREE.WebGLRenderer({ antialias: true, alpha: true, powerPreference: 'high-performance' });
    this.renderer.setPixelRatio(Math.min(window.devicePixelRatio || 1, 2));
    this.renderer.setSize(width, height);
    this.container.innerHTML = '';
    this.container.appendChild(this.renderer.domElement);

    // Lighting
    const ambient = new THREE.AmbientLight(0xffffff, 0.85);
    this.scene.add(ambient);

    const keyLight = new THREE.DirectionalLight(0xe0f2fe, 1.4);
    keyLight.position.set(3, 5, 4);
    this.scene.add(keyLight);

    const rimLight = new THREE.PointLight(0x38bdf8, 1.8, 8);
    rimLight.position.set(-2, 1.8, 2);
    this.scene.add(rimLight);

    this.staffGroup = new THREE.Group();
    this.scene.add(this.staffGroup);

    this.buildStaffGeometry(THREE);
    this.bindEvents();

    this.isInitialized = true;
    return true;
  }

  buildStaffGeometry(THREE) {
    this.interactiveMeshes = [];
    this.ledMeshes = [];

    // 1. Upper Clear Tapered Bell-Jar Dome (NO beads — direct view of concentric LED rings)
    const domeMat = new THREE.MeshPhysicalMaterial({
      color: 0xe0f2fe,
      transparent: true,
      opacity: 0.28,
      roughness: 0.08,
      metalness: 0.05,
      clearcoat: 1.0
    });
    const domeGeom = new THREE.SphereGeometry(1.12, 32, 24, 0, Math.PI * 2, 0, Math.PI * 0.58);
    const domeMesh = new THREE.Mesh(domeGeom, domeMat);
    domeMesh.position.set(0, 1.05, 0);
    domeMesh.userData.hotspotId = 'dome';
    this.staffGroup.add(domeMesh);
    this.interactiveMeshes.push(domeMesh);

    // 2. Forward-facing Concentric Black PCB Rings + 43 WS2812B LEDs inside the Clear Dome
    const ledGroup = new THREE.Group();
    ledGroup.position.set(-0.08, 1.16, 0.18);
    ledGroup.rotation.x = 0.22; // Upright, facing the viewer like the photograph
    this.staffGroup.add(ledGroup);

    const pcbRingMat = new THREE.MeshStandardMaterial({ color: 0x0f172a, roughness: 0.5 });
    const outerPcb = new THREE.Mesh(new THREE.RingGeometry(0.56, 0.78, 32), pcbRingMat);
    outerPcb.material.side = THREE.DoubleSide;
    ledGroup.add(outerPcb);
    const innerPcb = new THREE.Mesh(new THREE.CircleGeometry(0.48, 32), pcbRingMat);
    innerPcb.position.z = -0.01;
    ledGroup.add(innerPcb);

    const ledGeom = new THREE.BoxGeometry(0.075, 0.075, 0.035);
    this.store.ledNodes.forEach((node) => {
      const mat = new THREE.MeshBasicMaterial({ color: 0x38bdf8 });
      const mesh = new THREE.Mesh(ledGeom, mat);
      const nx = ((node.x - 155) / 122) * 0.72;
      const ny = ((node.y - 155) / 122) * 0.72;
      mesh.position.set(nx, -ny, 0.02);
      mesh.userData.hotspotId = 'leds';
      ledGroup.add(mesh);
      this.ledMeshes.push(mesh);
      this.interactiveMeshes.push(mesh);
    });

    // 3. Lower Transparent Rounded-Square Electronics Enclosure
    const boxGeom = new THREE.BoxGeometry(2.05, 1.85, 1.25);
    const boxMat = new THREE.MeshPhysicalMaterial({
      color: 0xcbd5e1,
      transparent: true,
      opacity: 0.26,
      roughness: 0.15,
      metalness: 0.05
    });
    const boxMesh = new THREE.Mesh(boxGeom, boxMat);
    boxMesh.position.set(0, -0.1, 0);
    boxMesh.userData.hotspotId = 'pico';
    this.staffGroup.add(boxMesh);
    this.interactiveMeshes.push(boxMesh);

    // Wireframe edge highlight for clear enclosure
    const edges = new THREE.EdgesGeometry(boxGeom);
    const lineMat = new THREE.LineBasicMaterial({ color: 0x64748b, transparent: true, opacity: 0.5 });
    const boxEdges = new THREE.LineSegments(edges, lineMat);
    boxEdges.position.copy(boxMesh.position);
    this.staffGroup.add(boxEdges);

    // 4. Vertical Right-Side White Breadboard & Horizontal Middle-Left Green Raspberry Pi Pico 2 W
    const bbMesh = new THREE.Mesh(
      new THREE.BoxGeometry(0.88, 1.22, 0.16),
      new THREE.MeshStandardMaterial({ color: 0xf1f5f9, roughness: 0.6 })
    );
    bbMesh.position.set(0.36, 0.08, -0.05);
    bbMesh.userData.hotspotId = 'pico';
    this.staffGroup.add(bbMesh);

    const picoMesh = new THREE.Mesh(
      new THREE.BoxGeometry(1.05, 0.44, 0.08),
      new THREE.MeshStandardMaterial({ color: 0x15803d, roughness: 0.4 })
    );
    picoMesh.position.set(-0.22, 0.12, 0.10);
    picoMesh.userData.hotspotId = 'pico';
    this.staffGroup.add(picoMesh);
    this.interactiveMeshes.push(picoMesh);

    // Glowing green onboard LED on the left end of the Pico 2 W
    const picoLed = new THREE.Mesh(
      new THREE.SphereGeometry(0.055, 12, 12),
      new THREE.MeshBasicMaterial({ color: 0x4ade80 })
    );
    picoLed.position.set(-0.62, 0.12, 0.16);
    this.staffGroup.add(picoLed);

    // 5. Clockwise-Tilted Blue 1.3" SH1106 OLED Display in Lower-Right
    if (this.oledCanvas) {
      this.oledTexture = new THREE.CanvasTexture(this.oledCanvas);
      this.oledTexture.minFilter = THREE.LinearFilter;
      this.oledTexture.magFilter = THREE.NearestFilter;
    }

    const oledCarrier = new THREE.Mesh(
      new THREE.BoxGeometry(0.92, 0.62, 0.06),
      new THREE.MeshStandardMaterial({ color: 0x0f3c78, roughness: 0.4 })
    );
    oledCarrier.position.set(0.38, -0.52, 0.42);
    oledCarrier.rotation.z = -0.20;
    oledCarrier.userData.hotspotId = 'oled';
    this.staffGroup.add(oledCarrier);
    this.interactiveMeshes.push(oledCarrier);

    const oledScreenMat = this.oledTexture
      ? new THREE.MeshBasicMaterial({ map: this.oledTexture })
      : new THREE.MeshBasicMaterial({ color: 0x38bdf8 });
    const oledScreen = new THREE.Mesh(new THREE.PlaneGeometry(0.82, 0.44), oledScreenMat);
    oledScreen.position.set(0, 0, 0.035);
    oledScreen.userData.hotspotId = 'oled';
    oledCarrier.add(oledScreen);

    // 6. Left-Side Bourns PEC11R Rotary Encoder + Small Green Side QUERY Button
    const encCylinder = new THREE.Mesh(
      new THREE.CylinderGeometry(0.08, 0.08, 0.55, 16),
      new THREE.MeshStandardMaterial({ color: 0x94a3b8, metalness: 0.7, roughness: 0.25 })
    );
    encCylinder.rotation.z = Math.PI / 2;
    encCylinder.position.set(-1.22, -0.10, 0);
    encCylinder.userData.hotspotId = 'encoder';
    this.staffGroup.add(encCylinder);
    this.interactiveMeshes.push(encCylinder);

    const greenQueryMesh = new THREE.Mesh(
      new THREE.CylinderGeometry(0.10, 0.10, 0.26, 16),
      new THREE.MeshStandardMaterial({ color: 0x22c55e, roughness: 0.3 })
    );
    greenQueryMesh.rotation.z = Math.PI / 2;
    greenQueryMesh.position.set(-1.12, -0.45, 0.12);
    greenQueryMesh.userData.hotspotId = 'query';
    this.staffGroup.add(greenQueryMesh);
    this.interactiveMeshes.push(greenQueryMesh);

    // 7. Bottom Yellow INMP441 I2S Microphone Module
    const kinderMesh = new THREE.Mesh(
      new THREE.CylinderGeometry(0.32, 0.32, 0.16, 24),
      new THREE.MeshStandardMaterial({ color: 0xeab308, roughness: 0.32 })
    );
    kinderMesh.rotation.x = Math.PI / 2;
    kinderMesh.position.set(0, -1.20, 0.18);
    kinderMesh.userData.hotspotId = 'kinder_mic';
    this.staffGroup.add(kinderMesh);
    this.interactiveMeshes.push(kinderMesh);

    // 9. Wooden Shaft (~28 mm hardwood handle)
    const shaftMesh = new THREE.Mesh(
      new THREE.CylinderGeometry(0.28, 0.28, 1.8, 24),
      new THREE.MeshStandardMaterial({ color: 0xb45309, roughness: 0.75 })
    );
    shaftMesh.position.set(0, -1.95, 0);
    shaftMesh.userData.hotspotId = 'shaft';
    this.staffGroup.add(shaftMesh);
    this.interactiveMeshes.push(shaftMesh);
  }

  bindEvents() {
    const dom = this.renderer.domElement;
    dom.addEventListener('pointerdown', (e) => {
      this.isDragging = true;
      this.prevPointer = { x: e.clientX, y: e.clientY };
    });

    window.addEventListener('pointermove', (e) => {
      if (!this.isDragging) return;
      const dx = e.clientX - this.prevPointer.x;
      const dy = e.clientY - this.prevPointer.y;
      this.rotY += dx * 0.008;
      this.rotX = Math.max(-0.6, Math.min(0.8, this.rotX + dy * 0.008));
      this.prevPointer = { x: e.clientX, y: e.clientY };
    });

    window.addEventListener('pointerup', () => {
      this.isDragging = false;
    });

    dom.addEventListener('wheel', (e) => {
      e.preventDefault();
      this.zoomDist = Math.max(4.0, Math.min(9.0, this.zoomDist + (e.deltaY > 0 ? 0.35 : -0.35)));
      if (this.camera) {
        this.camera.position.z = this.zoomDist;
      }
    }, { passive: false });

    dom.addEventListener('click', (e) => {
      if (!this.THREE || !this.camera) return;
      const rect = dom.getBoundingClientRect();
      const mouse = new this.THREE.Vector2(
        ((e.clientX - rect.left) / rect.width) * 2 - 1,
        -((e.clientY - rect.top) / rect.height) * 2 + 1
      );
      const raycaster = new this.THREE.Raycaster();
      raycaster.setFromCamera(mouse, this.camera);
      const hits = raycaster.intersectObjects(this.interactiveMeshes, true);
      if (hits.length > 0) {
        const id = hits[0].object.userData.hotspotId;
        if (id) {
          this.store.update({ selectedHotspotId: id });
          this.onSelectHotspot(id);
        }
      }
    });
  }

  render() {
    if (!this.isInitialized || !this.renderer || !this.scene || !this.camera) return;
    const state = this.store.getState();

    // Sync live 43 LED colors
    for (let i = 0; i < this.ledMeshes.length; i++) {
      const node = this.store.ledNodes[i];
      if (node && this.ledMeshes[i]) {
        this.ledMeshes[i].material.color.setRGB(node.r / 255, node.g / 255, node.b / 255);
      }
    }

    // Sync shared OLED canvas texture
    if (this.oledTexture) {
      this.oledTexture.needsUpdate = true;
    }

    if (this.staffGroup) {
      if (!this.isDragging && !state.reducedMotion) {
        this.rotY += 0.0025;
      }
      this.staffGroup.rotation.y = this.rotY;
      this.staffGroup.rotation.x = this.rotX;
    }

    this.renderer.render(this.scene, this.camera);
  }
}

