<template>
  <div class="col-8 col-sm-4 col-md-3 text-center">
    <button
      type="button"
      class="btn btn-sm btn-warning w-100"
      @click="reset_remote()"
      :disabled="resetting"
    >
      <span
        v-if="resetting"
        id="reset-spinner"
        class="spinner-border spinner-border-sm"
        role="status"
      ></span>
      <span v-else>Reset remote</span>
    </button>
  </div>
</template>

<script>
import axios from "axios";

export default {
  data() {
    return {
      resetting: false,
    };
  },

  emits: ["request_start", "request_success", "request_failure", "request_end"],

  methods: {
    reset_remote() {
      this.resetting = true;
      axios
        .post("/reset")
        .then(() => {
          this.$emit("request_success");
        })
        .catch(() => {
          this.$emit("request_failure");
        })
        .then(() => {
          this.$emit("request_end");
          this.resetting = false;
        });
    },
  },
};
</script>
