<template>
  <div
    v-if="text"
    class="alert alert-danger alert-dismissible position-fixed rolek-alert"
    role="alert"
  >
    {{ text }}
    <button type="button" class="btn-close" @click="dismiss"></button>
  </div>
</template>

<style>
.rolek-alert {
  position: fixed;
  top: 10px;
  left: 10%;
  width: 80%;
  z-index: 9999;
}
</style>

<script>
export default {
  data() {
    return {
      text: null,
      timeout: null,
    };
  },
  methods: {
    dismiss() {
      this.text = null;
      if (this.timeout) {
        clearTimeout(this.timeout);
        this.timeout = null;
      }
    },
    set(text) {
      this.dismiss();
      this.text = text;
      this.timeout = setTimeout(() => {
        this.dismiss();
      }, 5000);
    },
  },
};
</script>
